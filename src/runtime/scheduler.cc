#include "scheduler.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "overload.hh"
#include "storage_exception.hh"
#include "types.hh"
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

using namespace std;

bool is_local( shared_ptr<IRuntime> rt )
{
  return rt->get_info()->link_speed == numeric_limits<double>::max();
}

void get( shared_ptr<IRuntime> worker, Handle<AnyDataType> job, Relater& rt )
{
  if ( is_local( worker ) ) {
    job.visit<void>( overload {
      [&]( Handle<Literal> ) {},
      [&]( auto h ) { worker->get( h ); },
    } );
  } else {
    // Send visit( job ) before sending the job itself
    auto send = [&]( Handle<AnyDataType> h ) {
      h.visit<void>( overload { []( Handle<Literal> ) {},
                                []( Handle<Relation> ) {},
                                [&]( auto x ) { worker->put( x, rt.get( x ).value() ); } } );
    };

    job.visit<void>( overload {
      [&]( Handle<Relation> r ) {
        rt.visit( r.visit<Handle<Fix>>( overload { []( Handle<Apply> a ) { return a.unwrap<ObjectTree>(); },
                                                   []( Handle<Eval> e ) {
                                                     auto obj = e.unwrap<Object>();
                                                     return handle::extract<Thunk>( obj )
                                                       .transform( []( auto obj ) -> Handle<Fix> {
                                                         return Handle<Strict>( obj );
                                                       } )
                                                       .or_else( [&]() -> optional<Handle<Fix>> { return obj; } )
                                                       .value();
                                                   } } ),
                  send );
      },
      []( auto ) {} } );

    job.visit<void>( overload { []( Handle<Literal> ) { throw runtime_error( "Unreachable" ); },
                                [&]( auto h ) { worker->get( h ); } } );
  }
}

void LocalFirstScheduler::schedule( vector<Handle<AnyDataType>>& leaf_jobs, Handle<Relation> top_level_job )
{
  auto local = relater_.value().get().local_;
  if ( local->get_info()->parallelism > 0 ) {
    for ( const auto& leaf_job : leaf_jobs ) {
      get( local, leaf_job, relater_->get() );
    }
    return;
  }

  auto locked_remotes = relater_->get().remotes_.read();
  if ( locked_remotes->size() > 0 ) {
    for ( const auto& remote : locked_remotes.get() ) {
      auto locked_remote = remote.lock();
      if ( locked_remote ) {
        get( locked_remote, top_level_job, relater_->get() );
        return;
      }
    }
  }

  throw HandleNotFound( top_level_job );
}

void OnePassScheduler::schedule( vector<Handle<AnyDataType>>& leaf_jobs, Handle<Relation> top_level_job )
{
  // If all dependencies are resolved, the job should have been completed
  if ( leaf_jobs.empty() ) {
    throw runtime_error( "Invalid schedule() invocation." );
    return;
  }

  auto location = schedule_rec( top_level_job );
  if ( !is_local( location ) ) {
    get( location, top_level_job, relater_->get() );
  }
}

shared_ptr<IRuntime> OnePassScheduler::schedule_rec( Handle<AnyDataType> top_level_job )
{
  return top_level_job.visit<shared_ptr<IRuntime>>(
    overload { [&]( Handle<Relation> r ) {
                return r.visit<shared_ptr<IRuntime>>(
                  overload { [&]( Handle<Apply> a ) {
                              // Leaf job: apply
                              auto loc = locate( a, {} );
                              if ( is_local( loc ) ) {
                                loc->get( a );
                              }
                              return loc;
                            },
                             [&]( Handle<Eval> e ) {
                               // Recursive case
                               auto dependencies = relater_->get().graph_.read()->get_forward_dependencies( e );
                               unordered_map<Handle<AnyDataType>, shared_ptr<IRuntime>> dependency_locations;
                               for ( const auto& dependency : dependencies ) {
                                 dependency_locations.insert( { dependency, schedule_rec( dependency ) } );
                               }
                               auto location = locate( top_level_job, dependency_locations );

                               if ( is_local( location ) ) {
                                 // Get any non-local child work
                                 for ( const auto& [d, dloc] : dependency_locations ) {
                                   if ( !is_local( dloc ) ) {
                                     get( dloc, d, relater_->get() );
                                   }
                                 }
                               };

                               return location;
                             } } );
              },
               []( Handle<Literal> ) -> shared_ptr<IRuntime> { throw std::runtime_error( "Unreachable" ); },
               [&]( auto h ) {
                 // Leaf job: data
                 auto loc = locate( h, {} );
                 if ( is_local( loc ) ) {
                   loc->get( h );
                 }
                 return loc;
               } } );
}

// Calculate absent size from a root assuming that rt contains the whole object
size_t absent_size( Relater& rt, shared_ptr<IRuntime> worker, Handle<Fix> root )
{
  size_t absent_size = 0;
  rt.early_stop_visit_minrepo( root, [&]( Handle<AnyDataType> handle ) -> bool {
    auto contained = handle.visit<bool>(
      overload { [&]( Handle<Literal> ) { return true; }, [&]( auto h ) { return worker->contains( h ); } } );
    if ( !contained ) {
      absent_size += handle::size( handle );
    }

    return contained;
  } );
  return absent_size;
}

// Calculate a "score" for a worker, the lower the better
size_t score( Relater& rt,
              shared_ptr<IRuntime> worker,
              Handle<AnyDataType> top_level_job,
              const unordered_map<Handle<AnyDataType>, shared_ptr<IRuntime>>& dependency_locations )
{
  Handle<Fix> root = top_level_job.visit<Handle<Fix>>(
    overload { [&]( Handle<Relation> r ) {
                return r.visit<Handle<Fix>>( overload {

                  [&]( Handle<Apply> a ) { return a.unwrap<ObjectTree>(); },
                  [&]( Handle<Eval> e ) {
                    return handle::extract<Identification>( e.unwrap<Object>() )
                      .transform( []( auto h ) -> Handle<Fix> { return h.template unwrap<Value>(); } )
                      .or_else( [&]() -> optional<Handle<Fix>> { return e.unwrap<Object>(); } )
                      .value();
                  } } );
              },
               [&]( Handle<AnyTree> h ) { return handle::fix( h ); },
               [&]( auto h ) { return h; } } );

  size_t score = absent_size( rt, worker, root );

  for ( auto& [task, location] : dependency_locations ) {
    if ( worker != location ) {
      auto dependency_size = const_cast<Handle<AnyDataType>&>( task ).visit<size_t>(
        overload { [&]( Handle<Relation> r ) -> size_t {
                    return r.visit<size_t>(
                      overload { [&]( Handle<Apply> a ) {
                                  auto rlimits = rt.get( a.unwrap<ObjectTree>() ).value()->at( 0 );
                                  auto limits = rt.get( handle::extract<ValueTree>( rlimits ).value() ).value();
                                  auto output_size = handle::extract<Literal>( limits->at( 1 ) )
                                                       .transform( [&]( auto x ) { return uint64_t( x ); } )
                                                       .value_or( 0 );
                                  return output_size;
                                },
                                 [&]( Handle<Eval> e ) -> size_t {
                                   return handle::extract<Identification>( e.unwrap<Object>() )
                                     .transform( [&]( auto h ) -> size_t {
                                       return absent_size( rt, worker, h.template unwrap<Value>() );
                                     } )
                                     .or_else( []() -> optional<size_t> { return 0; } )
                                     .value();
                                 } } );
                  },
                   [&]( auto h ) { return absent_size( rt, worker, h ); } } );

      score += dependency_size;
    }
  }

  top_level_job.visit<void>(
    overload { [&]( Handle<Relation> r ) {
                return r.visit<void>(
                  overload { [&]( Handle<Apply> a ) {
                              auto rlimits = rt.get( a.unwrap<ObjectTree>() ).value()->at( 0 );
                              auto limits = rt.get( handle::extract<ValueTree>( rlimits ).value() ).value();
                              auto output_fan_out = handle::extract<Literal>( limits->at( 2 ) )
                                                      .transform( [&]( auto x ) { return uint64_t( x ); } )
                                                      .value_or( 0 );

                              if ( worker->get_info()->parallelism < output_fan_out ) {
                                score = score * ( ceil( 1.0 * output_fan_out / worker->get_info()->parallelism ) );
                              }
                            },
                             [&]( Handle<Eval> ) {} } );
              },
               []( auto ) {} } );

  return score;
}

shared_ptr<IRuntime> OnePassScheduler::locate(
  Handle<AnyDataType> top_level_job,
  const unordered_map<Handle<AnyDataType>, shared_ptr<IRuntime>>& dependency_locations )
{
  unordered_map<shared_ptr<IRuntime>, size_t> scores;

  auto local = relater_.value().get().local_;
  if ( local->get_info().has_value() and local->get_info()->parallelism > 0 ) {
    scores.insert( { local, score( relater_->get(), local, top_level_job, dependency_locations ) } );
  }

  for ( const auto& remote : relater_->get().remotes_.read().get() ) {
    auto locked_remote = remote.lock();
    // If remote already contains the result
    auto contained = top_level_job.visit<bool>(
      overload { []( Handle<Literal> ) -> bool { throw std::runtime_error( "Unreachable" ); },
                 [&]( auto h ) { return locked_remote->contains( h ); } } );

    if ( contained ) {
      return locked_remote;
    }

    if ( locked_remote->get_info().has_value() and locked_remote->get_info()->parallelism > 0 ) {
      scores.insert(
        { locked_remote, score( relater_->get(), locked_remote, top_level_job, dependency_locations ) } );
    }
  }

  // Choose the remote with the most parallelism among ones with smallest score
  size_t min_absent_size = numeric_limits<size_t>::max();
  size_t max_parallelism = 0;

  shared_ptr<IRuntime> chosen_remote;

  for ( const auto& [location, absent_size] : scores ) {
    if ( absent_size < min_absent_size ) {
      min_absent_size = absent_size;
      max_parallelism = location->get_info()->parallelism;
      chosen_remote = location;
    } else if ( absent_size == min_absent_size && location->get_info()->parallelism > max_parallelism ) {
      max_parallelism = location->get_info()->parallelism;
      chosen_remote = location;
    }
  }

  return chosen_remote;
}
