#include "pass.hh"
#include "handle.hh"
#include "overload.hh"
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

Pass::Pass( reference_wrapper<Relater> relater )
  : relater_( relater )
{}

void Pass::run( Handle<AnyDataType> job )
{
  job.visit<void>( overload { [&]( Handle<Relation> r ) {
                               return r.visit<void>( overload {
                                 [&]( Handle<Apply> a ) {
                                   independent( a );
                                   leaf( a );
                                 },
                                 [&]( Handle<Eval> e ) {
                                   auto dependencies = relater_.get().graph_.read()->get_forward_dependencies( e );
                                   independent( e );
                                   pre( e, dependencies );
                                   for ( const auto& dependency : dependencies ) {
                                     run( dependency );
                                   }
                                   post( e, dependencies );
                                 },
                               } );
                             },
                              []( Handle<Literal> ) { throw std::runtime_error( "Unreachable" ); },
                              [&]( auto h ) {
                                independent( h );
                                leaf( h );
                              } } );
}

void PrunedSelectionPass::run( Handle<AnyDataType> job )
{
  if ( !is_local( chosen_remotes_.at( job ) ) ) {
    independent( job );
    return;
  }

  job.visit<void>( overload { [&]( Handle<Relation> r ) {
                               return r.visit<void>( overload {
                                 [&]( Handle<Apply> a ) {
                                   independent( a );
                                   leaf( a );
                                 },
                                 [&]( Handle<Eval> e ) {
                                   auto dependencies = relater_.get().graph_.read()->get_forward_dependencies( e );
                                   independent( e );
                                   pre( e, dependencies );
                                   for ( const auto& dependency : dependencies ) {
                                     if ( !is_local( chosen_remotes_.at( job ) ) ) {
                                       run( dependency );
                                     }
                                   }
                                   post( e, dependencies );
                                 },
                               } );
                             },
                              []( Handle<Literal> ) { throw std::runtime_error( "Unreachable" ); },
                              [&]( auto h ) {
                                independent( h );
                                leaf( h );
                              } } );
}

BasePass::BasePass( reference_wrapper<Relater> relater )
  : Pass( relater )
{}

void BasePass::leaf( Handle<AnyDataType> job )
{
  job.visit<void>( overload { [&]( Handle<Relation> r ) {
                               return r.visit<void>( overload {
                                 [&]( Handle<Apply> a ) {
                                   auto rlimits = relater_.get().get( a.unwrap<ObjectTree>() ).value()->at( 0 );
                                   auto limits
                                     = relater_.get().get( handle::extract<ValueTree>( rlimits ).value() ).value();
                                   auto output_size = handle::extract<Literal>( limits->at( 1 ) )
                                                        .transform( [&]( auto x ) { return uint64_t( x ); } )
                                                        .value_or( 0 );
                                   auto output_fan_out = handle::extract<Literal>( limits->at( 2 ) )
                                                           .transform( [&]( auto x ) { return uint64_t( x ); } )
                                                           .value_or( 1 );

                                   tasks_info_[job].in_out_size = { sizeof( Handle<Fix> ), output_size };
                                   tasks_info_[job].output_fan_out = output_fan_out;
                                 },
                                 [&]( Handle<Eval> ) {},
                               } );
                             },
                              []( Handle<Literal> ) { throw std::runtime_error( "Unreachable" ); },
                              [&]( auto h ) {
                                tasks_info_[job].in_out_size = { sizeof( Handle<Fix> ), handle::size( h ) };
                                tasks_info_[job].output_fan_out = 0;
                              } } );
}

void BasePass::post( Handle<AnyDataType> job, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  // Output size = input_size + replacing every dependent's input with output
  // Output fan out = sum( dependents' fan out )
  auto input_size = handle::size( job );
  auto output_size = input_size;
  size_t output_fan_out = 0;
  for ( const auto& dependency : dependencies ) {
    auto [in, out] = tasks_info_.at( dependency ).in_out_size;
    output_size = output_size - in + out;
    output_fan_out += tasks_info_.at( dependency ).output_fan_out;
  }
  tasks_info_[job].in_out_size = { input_size, output_size };
  tasks_info_[job].output_fan_out = output_fan_out;
}

Handle<Fix> get_root( Handle<AnyDataType> job )
{
  return job.visit<Handle<Fix>>(
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
}

size_t BasePass::absent_size( std::shared_ptr<IRuntime> worker, Handle<AnyDataType> job )
{
  auto root = get_root( job );
  size_t contained_size = 0;
  relater_.get().early_stop_visit_minrepo( root, [&]( Handle<AnyDataType> handle ) -> bool {
    auto contained = handle.visit<bool>(
      overload { [&]( Handle<Literal> ) { return true; }, [&]( auto h ) { return worker->contains( h ); } } );
    if ( contained ) {
      contained_size += handle::size( handle );
    }

    return contained;
  } );
  return handle::size( root ) - contained_size;
}

void BasePass::independent( Handle<AnyDataType> job )
{
  {
    auto local = relater_.get().local_;
    if ( local->get_info().has_value() and local->get_info()->parallelism > 0 ) {
      tasks_info_[job].absent_size.insert( { local, absent_size( local, job ) } );
    }
  }

  for ( const auto& remote : relater_.get().remotes_.read().get() ) {
    auto locked_remote = remote.lock();

    if ( locked_remote->get_info().has_value() and locked_remote->get_info()->parallelism > 0 ) {
      tasks_info_[job].absent_size.insert( { locked_remote, absent_size( locked_remote, job ) } );
      job.visit<void>( overload { [&]( Handle<Relation> r ) {
                                   if ( locked_remote->contains( r ) ) {
                                     tasks_info_[job].contains.insert( locked_remote );
                                   }
                                 },
                                  []( auto ) {} } );
    }
  }
}

SelectionPass::SelectionPass( reference_wrapper<BasePass> base, reference_wrapper<Relater> relater )
  : Pass( relater )
  , base_( base )
  , chosen_remotes_()
{}

SelectionPass::SelectionPass( std::reference_wrapper<BasePass> base,
                              std::reference_wrapper<Relater> relater,
                              SelectionPass& prev )
  : Pass( relater )
  , base_( base )
  , chosen_remotes_( std::move( prev.release() ) )
{}

PrunedSelectionPass::PrunedSelectionPass( std::reference_wrapper<BasePass> base,
                                          std::reference_wrapper<Relater> relater,
                                          SelectionPass& prev )
  : SelectionPass( base, relater, prev )
{}

void MinAbsentMaxParallelism::leaf( Handle<AnyDataType> job )
{
  optional<shared_ptr<IRuntime>> chosen_remote;

  job.visit<void>( overload { [&]( Handle<Relation> r ) {
                               const auto& contains = base_.get().get_contains( r );
                               if ( !contains.empty() ) {
                                 chosen_remote = *contains.begin();
                               }
                             },
                              []( auto ) {} } );

  if ( !chosen_remote.has_value() ) {
    const auto& absent_size = base_.get().get_absent_size( job );

    size_t min_absent_size = numeric_limits<size_t>::max();
    size_t max_parallelism = 0;

    for ( const auto& [r, s] : absent_size ) {
      if ( s < min_absent_size ) {
        min_absent_size = s;
        max_parallelism = r->get_info()->parallelism;
        chosen_remote = r;
      } else if ( s == min_absent_size && r->get_info()->parallelism > max_parallelism ) {
        max_parallelism = r->get_info()->parallelism;
        chosen_remote = r;
      }
    }
  }

  chosen_remotes_.insert( { job, chosen_remote.value() } );
}

void MinAbsentMaxParallelism::post( Handle<AnyDataType> job,
                                    const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  optional<shared_ptr<IRuntime>> chosen_remote;

  job.visit<void>( overload { [&]( Handle<Relation> r ) {
                               const auto& contains = base_.get().get_contains( r );
                               if ( !contains.empty() ) {
                                 chosen_remote = *contains.begin();
                               }
                             },
                              []( auto ) {} } );

  if ( !chosen_remote.has_value() ) {
    absl::flat_hash_map<shared_ptr<IRuntime>, size_t> present_output;

    for ( const auto& d : dependencies ) {
      present_output[chosen_remotes_.at( d )] += base_.get().get_in_out_size( d ).second;
      // TODO: handle data and load differently
    }

    const auto& absent_size = base_.get().get_absent_size( job );

    int64_t min_absent_size = numeric_limits<int64_t>::max();
    size_t max_parallelism = 0;

    for ( const auto& [r, s] : absent_size ) {
      int64_t sum_size = s - present_output[r];
      if ( sum_size < min_absent_size ) {
        min_absent_size = sum_size;
        max_parallelism = r->get_info()->parallelism;
        chosen_remote = r;
      } else if ( sum_size == min_absent_size && r->get_info()->parallelism > max_parallelism ) {
        max_parallelism = r->get_info()->parallelism;
        chosen_remote = r;
      }
    }
  }

  chosen_remotes_.insert( { job, chosen_remote.value() } );
}

void ChildBackProp::independent( Handle<AnyDataType> job )
{
  auto dependee = dependees_.at( job );

  unordered_set<shared_ptr<IRuntime>> move_output;
  for ( const auto& d : dependee ) {
    move_output.insert( chosen_remotes_.at( d ) );
  }

  optional<shared_ptr<IRuntime>> chosen_remote;

  job.visit<void>( overload { [&]( Handle<Relation> r ) {
                               const auto& contains = base_.get().get_contains( r );
                               if ( !contains.empty() ) {
                                 for ( const auto& c : contains ) {
                                   if ( move_output.contains( c ) ) {
                                     chosen_remote = c;
                                     return;
                                   }
                                 }
                                 chosen_remote = *contains.begin();
                               }
                             },
                              []( auto ) {} } );

  if ( !chosen_remote.has_value() ) {

    const auto& absent_size = base_.get().get_absent_size( job );

    size_t min_absent_size = numeric_limits<int64_t>::max();
    size_t max_parallelism = 0;

    for ( const auto& [r, s] : absent_size ) {
      size_t sum_size = s + ( move_output.contains( r ) ? base_.get().get_in_out_size( job ).second : 0 );
      if ( sum_size < min_absent_size ) {
        min_absent_size = sum_size;
        max_parallelism = r->get_info()->parallelism;
        chosen_remote = r;
      } else if ( sum_size == min_absent_size && r->get_info()->parallelism > max_parallelism ) {
        max_parallelism = r->get_info()->parallelism;
        chosen_remote = r;
      }
    }
  }

  chosen_remotes_.insert( { job, chosen_remote.value() } );
}

void ChildBackProp::pre( Handle<AnyDataType> job, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  for ( const auto& d : dependencies ) {
    dependees_[d].insert( job );
  }
}

void Parallelize::pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  unordered_map<shared_ptr<IRuntime>, size_t> assigned_parallelism;

  for ( auto d : dependencies ) {
    auto chosen = chosen_remotes_.at( d );
    auto& assigned = assigned_parallelism[chosen_remotes_.at( d )];

    auto contained = d.visit<bool>(
      overload { []( Handle<Literal> ) { return true; }, [&]( auto h ) { return chosen->contains( h ); } } );
    if ( contained )
      continue;

    if ( assigned >= chosen->get_info()->parallelism ) {
      // Choose a different remote for this dependency
      // XXX: calculate score by queue length and data movement cost
      const auto& absent_size = base_.get().get_absent_size( d );

      size_t min_absent_size = numeric_limits<size_t>::max();
      size_t max_parallelism = 0;

      shared_ptr<IRuntime> chosen_remote;

      for ( const auto& [r, s] : absent_size ) {
        if ( assigned_parallelism[r] >= chosen->get_info()->parallelism ) {
          continue;
        }

        if ( s < min_absent_size ) {
          min_absent_size = s;
          max_parallelism = r->get_info()->parallelism;
          chosen_remote = r;
        } else if ( s == min_absent_size && r->get_info()->parallelism > max_parallelism ) {
          max_parallelism = r->get_info()->parallelism;
          chosen_remote = r;
        }
      }
      chosen_remotes_.insert( { d, chosen_remote } );
    } else {
      assigned += base_.get().get_fan_out( d );
    }
  }
}

void FinalPass::leaf( Handle<AnyDataType> job )
{
  get( chosen_remotes_.at( job ), job, relater_.get() );
}

void FinalPass::independent( Handle<AnyDataType> job )
{
  if ( !is_local( chosen_remotes_.at( job ) ) ) {
    get( chosen_remotes_.at( job ), job, relater_.get() );
  }
}

void FinalPass::pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  for ( const auto& d : dependencies ) {
    if ( !is_local( chosen_remotes_.at( d ) ) ) {
      get( chosen_remotes_.at( d ), d, relater_.get() );
    }
  }
}

void PassRunner::run( reference_wrapper<Relater> rt, Handle<AnyDataType> top_level_job, vector<PassType> passes )
{
  BasePass base( rt );
  base.run( top_level_job );

  optional<unique_ptr<SelectionPass>> selection;
  for ( const auto& pass : passes ) {
    switch ( pass ) {
      case PassType::MinAbsentMaxParallelism: {
        if ( selection.has_value() ) {
          selection = make_unique<MinAbsentMaxParallelism>( base, rt, *selection.value() );
        } else {
          selection = make_unique<MinAbsentMaxParallelism>( base, rt );
        }
        selection.value()->run( top_level_job );
        break;
      }

      case PassType::ChildPackProp: {
        if ( selection.has_value() ) {
          selection = make_unique<ChildBackProp>( base, rt, *selection.value() );
        } else {
          selection = make_unique<ChildBackProp>( base, rt );
        }
        selection.value()->run( top_level_job );
        break;
      }

      case PassType::Parallelize: {
        if ( selection.has_value() ) {
          selection = make_unique<Parallelize>( base, rt, *selection.value() );
        } else {
          throw runtime_error( "Invalid pass sequence." );
        }
        selection.value()->run( top_level_job );
        break;
      }
    }
  }

  if ( not selection.has_value() ) {
    throw runtime_error( "Invalid pass sequence." );
  }

  FinalPass final( base, rt, *selection.value() );
  final.run( top_level_job );
}
