#include "scheduler.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "storage_exception.hh"
#include "types.hh"
#include <limits>
#include <optional>

using namespace std;

bool is_local( shared_ptr<IRuntime> rt )
{
  return rt->get_info()->link_speed == numeric_limits<double>::max();
}

void get( shared_ptr<IRuntime> worker, Handle<Relation> job, Relater& rt )
{
  if ( is_local( worker ) ) {
    worker->get( job );
  } else {
    // Send visit( job ) before sending the job itself
    auto send = [&]( Handle<AnyDataType> h ) {
      h.visit<void>( overload { []( Handle<Literal> ) {},
                                []( Handle<Relation> ) {},
                                [&]( auto x ) { worker->put( x, rt.get( x ).value() ); } } );
    };

    rt.visit( job.visit<Handle<Fix>>( overload { []( Handle<Apply> a ) { return a.unwrap<ObjectTree>(); },
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

    worker->get( job );
  }
}

void LocalFirstScheduler::schedule( SharedMutex<vector<weak_ptr<IRuntime>>>& remotes,
                                    shared_ptr<IRuntime> local,
                                    [[maybe_unused]] SharedMutex<DependencyGraph>& graph,
                                    vector<Handle<Relation>>& leaf_jobs,
                                    Handle<Relation> top_level_job,
                                    Relater& rt )
{
  if ( local->get_info()->parallelism > 0 ) {
    for ( const auto& leaf_job : leaf_jobs ) {
      get( local, leaf_job, rt );
    }
    return;
  }

  auto locked_remotes = remotes.read();
  if ( locked_remotes->size() > 0 ) {
    for ( const auto& remote : locked_remotes.get() ) {
      auto locked_remote = remote.lock();
      if ( locked_remote ) {
        get( locked_remote, top_level_job, rt );
        return;
      }
    }
  }

  throw HandleNotFound( top_level_job );
}

void OnePassScheduler::schedule( SharedMutex<vector<weak_ptr<IRuntime>>>& remotes,
                                 shared_ptr<IRuntime> local,
                                 SharedMutex<DependencyGraph>& graph,
                                 vector<Handle<Relation>>& leaf_jobs,
                                 Handle<Relation> top_level_job,
                                 Relater& rt )
{
  // If all dependencies are resolved, the job should have been completed
  if ( leaf_jobs.empty() ) {
    throw runtime_error( "Invalid schedule() invocation." );
    return;
  }

  auto location = schedule_rec( remotes, local, graph, top_level_job, rt );
  if ( !is_local( location ) ) {
    get( location, top_level_job, rt );
  }
}

shared_ptr<IRuntime> OnePassScheduler::schedule_rec( SharedMutex<vector<weak_ptr<IRuntime>>>& remotes,
                                                     shared_ptr<IRuntime> local,
                                                     SharedMutex<DependencyGraph>& graph,
                                                     Handle<Relation> top_level_job,
                                                     Relater& rt )
{
  return top_level_job.visit<shared_ptr<IRuntime>>( overload {
    [&]( Handle<Apply> a ) {
      // Base case: Apply
      auto loc = locate( remotes, local, a, rt );
      if ( is_local( loc ) ) {
        loc->get( a );
      }
      return loc;
    },
    [&]( Handle<Eval> e ) {
      return handle::extract<Identification>( e.unwrap<Object>() )
        .transform( [&]( auto ) {
          // Base case: Eval( Identification )
          auto loc = locate( remotes, local, e, rt );
          if ( is_local( loc ) ) {
            loc->get( e );
          }
          return loc;
        } )
        .or_else( [&]() -> optional<shared_ptr<IRuntime>> {
          // Recursive case
          auto dependencies = graph.read()->get_forward_dependencies( top_level_job );
          unordered_map<Handle<Relation>, shared_ptr<IRuntime>> dependency_locations;
          for ( const auto& dependency : dependencies ) {
            dependency_locations.insert( { dependency, schedule_rec( remotes, local, graph, dependency, rt ) } );
          }
          auto location = locate( remotes, local, top_level_job, rt, dependency_locations );

          if ( is_local( location ) ) {
            // Get any non-local child work
            for ( const auto& [d, dloc] : dependency_locations ) {
              if ( !is_local( dloc ) ) {
                get( dloc, d, rt );
              }
            }
          };

          return location;
        } )
        .value();
    } } );
}

shared_ptr<IRuntime> OnePassScheduler::locate(
  SharedMutex<vector<weak_ptr<IRuntime>>>& remotes,
  shared_ptr<IRuntime> local,
  Handle<Relation> top_level_job,
  Relater& rt,
  unordered_map<Handle<Relation>, shared_ptr<IRuntime>> dependency_locations )
{
  if ( local->get_info()->parallelism > 0 ) {
    return local;
  }

  Handle<Fix> root = top_level_job.visit<Handle<Fix>>(
    overload { [&]( Handle<Apply> a ) { return a.unwrap<ObjectTree>(); },
               [&]( Handle<Eval> e ) {
                 return handle::extract<Identification>( e.unwrap<Object>() )
                   .transform( []( auto h ) -> Handle<Fix> { return h.template unwrap<Value>(); } )
                   .or_else( [&]() -> optional<Handle<Fix>> { return e.unwrap<Object>(); } )
                   .value();
               } } );

  unordered_map<shared_ptr<IRuntime>, size_t> absent_data;

  // For a remote, absent_sizes is the sum of absent Blob/Tree + absent load + absent apply result size
  for ( const auto& remote : remotes.read().get() ) {
    auto locked_remote = remote.lock();
    // If remote already contains the result
    if ( locked_remote->contains( top_level_job ) ) {
      return locked_remote;
    }

    if ( locked_remote->get_info()->parallelism > 0 ) {
      size_t absent_size = 0;
      rt.early_stop_visit_minrepo( root, [&]( Handle<AnyDataType> handle ) -> bool {
        auto contained = handle.visit<bool>( overload { [&]( Handle<Literal> ) { return true; },
                                                        [&]( auto h ) { return locked_remote->contains( h ); } } );

        if ( !contained ) {
          absent_size += handle::size( handle );
        }

        return contained;
      } );

      for ( auto& [task, location] : dependency_locations ) {
        if ( locked_remote != location ) {
          auto dependency_size = const_cast<Handle<Relation>&>( task ).visit<size_t>(
            overload { [&]( Handle<Apply> ) {
                        // TODO: estimate apply output size
                        return 0;
                      },
                       [&]( Handle<Eval> e ) -> size_t {
                         return handle::extract<Identification>( e.unwrap<Object>() )
                           .transform( []( auto h ) -> size_t { return handle::size( h ); } )
                           .or_else( []() -> optional<size_t> { return 0; } )
                           .value();
                       } } );

          absent_size += dependency_size;
        }
      }

      absent_data.insert( { locked_remote, absent_size } );
    }
  }

  // Choose the remote with the most parallelism among ones with smallest absent data
  size_t min_absent_size = numeric_limits<size_t>::max();
  size_t max_parallelism = 0;

  shared_ptr<IRuntime> chosen_remote;

  for ( const auto& [location, absent_size] : absent_data ) {
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
