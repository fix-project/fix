#include "pass.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "overload.hh"
#include <stdexcept>

using namespace std;

bool is_local( shared_ptr<IRuntime> rt )
{
  return rt->get_info()->link_speed == numeric_limits<double>::max();
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
  if ( !is_local( chosen_remotes_.at( job ).first ) ) {
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
                                     if ( is_local( chosen_remotes_.at( dependency ).first ) ) {
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
                                                        .value_or( 1 );
                                   auto output_fan_out = handle::extract<Literal>( limits->at( 2 ) )
                                                           .transform( [&]( auto x ) { return uint64_t( x ); } )
                                                           .value_or( 1 );

                                   tasks_info_[job].output_size = output_size;
                                   tasks_info_[job].output_fan_out = output_fan_out;
                                 },
                                 [&]( Handle<Eval> ) {},
                               } );
                             },
                              []( Handle<Literal> ) { throw std::runtime_error( "Unreachable" ); },
                              [&]( auto h ) {
                                tasks_info_[job].output_size = handle::size( h );
                                tasks_info_[job].output_fan_out = 0;
                              } } );
}

void BasePass::post( Handle<AnyDataType> job, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  // Output size = input_size + replacing every dependent's input with output
  Handle<Object> obj = job.unwrap<Relation>().unwrap<Eval>().unwrap<Object>();
  auto input_size = handle::size( obj );

  size_t output_size = input_size;
  size_t output_fan_out = 0;
  for ( const auto& dependency : dependencies ) {
    output_fan_out += tasks_info_.at( dependency ).output_fan_out;
    auto out = tasks_info_.at( dependency ).output_size;
    output_size += out;
  }

  auto outs = obj.visit<pair<size_t, size_t>>( overload {
    [&]( Handle<Thunk> t ) {
      return t.visit<pair<size_t, size_t>>( overload {
        [&]( Handle<Application> a ) -> pair<size_t, size_t> {
          if ( relater_.get().contains( a.unwrap<ExpressionTree>() ) ) {
            auto rlimits = relater_.get().get( a.unwrap<ExpressionTree>() ).value()->at( 0 );
            auto limits
              = handle::extract<ValueTree>( rlimits ).and_then( [&]( auto x ) { return relater_.get().get( x ); } );

            return { limits.and_then( [&]( auto x ) { return handle::extract<Literal>( x->at( 1 ) ); } )
                       .transform( [&]( auto x ) { return uint64_t( x ); } )
                       .value_or( output_size ),
                     max( limits.and_then( [&]( auto x ) { return handle::extract<Literal>( x->at( 2 ) ); } )
                            .transform( [&]( auto x ) { return uint64_t( x ); } )
                            .value_or( output_fan_out ),
                          output_fan_out ) };
          } else {
            return { output_size, output_fan_out };
          }
        },
        [&]( Handle<Identification> ) -> pair<size_t, size_t> {
          auto out = tasks_info_.at( *dependencies.begin() ).output_size;
          return { out, 1 };
        },
        [&]( Handle<Selection> ) -> pair<size_t, size_t> { throw std::runtime_error( "Unimplemented" ); } } );
    },
    [&]( auto ) -> pair<size_t, size_t> {
      return { output_size, output_fan_out };
    } } );

  tasks_info_[job].output_size = outs.first;
  tasks_info_[job].output_fan_out = outs.second;
}

size_t BasePass::absent_size( std::shared_ptr<IRuntime> worker, Handle<AnyDataType> job )
{
  auto root = job::get_root( job );
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
      VLOG( 2 ) << "local absent_size " << job << " " << tasks_info_[job].absent_size.at( local );
    }
  }

  for ( const auto& remote : relater_.get().remotes_.read().get() ) {
    auto locked_remote = remote.lock();
    if ( locked_remote->get_info().has_value() and locked_remote->get_info()->parallelism > 0 ) {
      tasks_info_[job].absent_size.insert( { locked_remote, absent_size( locked_remote, job ) } );
      VLOG( 2 ) << "remote absent_size " << job << " " << tasks_info_[job].absent_size.at( locked_remote );
      job.visit<void>( overload { [&]( auto r ) {
                                   if ( locked_remote->contains( r ) ) {
                                     tasks_info_[job].contains.insert( locked_remote );
                                   }
                                 },
                                  []( Handle<Literal> ) {} } );
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
                              std::unique_ptr<SelectionPass> prev )
  : Pass( relater )
  , base_( base )
  , chosen_remotes_( std::move( prev->release() ) )
{}

PrunedSelectionPass::PrunedSelectionPass( std::reference_wrapper<BasePass> base,
                                          std::reference_wrapper<Relater> relater,
                                          std::unique_ptr<SelectionPass> prev )
  : SelectionPass( base, relater, move( prev ) )
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

    size_t max_parallelism = 0;
    size_t min_absent_size = numeric_limits<size_t>::max();
    int64_t min_absent_size_diff = 0;

    for ( const auto& [r, s] : absent_size ) {
      if ( s < min_absent_size ) {
        min_absent_size_diff = s - min_absent_size;
        min_absent_size = s;
        max_parallelism = r->get_info()->parallelism;
        chosen_remote = r;
      } else if ( s == min_absent_size && r->get_info()->parallelism > max_parallelism ) {
        max_parallelism = r->get_info()->parallelism;
        chosen_remote = r;
      } else if ( s == min_absent_size && r->get_info()->parallelism == max_parallelism && is_local( r ) ) {
        chosen_remote = r;
      }
    }
    VLOG( 2 ) << "MinAbsent::leaf " << job << " "
              << ( is_local( chosen_remote.value() ) ? " locally" : " remotely" );
    chosen_remotes_.insert_or_assign( job, { chosen_remote.value(), min_absent_size_diff } );
  } else {
    VLOG( 2 ) << "MinAbsent::leaf " << job << " "
              << ( is_local( chosen_remote.value() ) ? " locally" : " remotely" );
    chosen_remotes_.insert_or_assign( job, { chosen_remote.value(), 0 } );
  }
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
      present_output[chosen_remotes_.at( d ).first] += base_.get().get_output_size( d );
      // TODO: handle data and load differently
    }

    const auto& absent_size = base_.get().get_absent_size( job );

    int64_t min_absent_size = numeric_limits<int64_t>::max();
    size_t max_parallelism = 0;
    int64_t min_absent_size_diff = 0;

    for ( const auto& [r, s] : absent_size ) {
      int64_t sum_size = s - present_output[r];
      if ( sum_size < min_absent_size ) {
        min_absent_size_diff = sum_size - min_absent_size;
        min_absent_size = sum_size;
        max_parallelism = r->get_info()->parallelism;
        chosen_remote = r;
      } else if ( sum_size == min_absent_size && r->get_info()->parallelism > max_parallelism ) {
        max_parallelism = r->get_info()->parallelism;
        chosen_remote = r;
      } else if ( sum_size == min_absent_size && r->get_info()->parallelism == max_parallelism && is_local( r ) ) {
        chosen_remote = r;
      }
    }
    VLOG( 2 ) << "MinAbsent::post " << job << " "
              << ( is_local( chosen_remote.value() ) ? " locally" : " remotely" );
    chosen_remotes_.insert_or_assign( job, { chosen_remote.value(), min_absent_size_diff } );
  } else {
    VLOG( 2 ) << "MinAbsent::post " << job << " "
              << ( is_local( chosen_remote.value() ) ? " locally" : " remotely" );
    chosen_remotes_.insert_or_assign( job, { chosen_remote.value(), 0 } );
  }
}

void ChildBackProp::independent( Handle<AnyDataType> job )
{
  VLOG( 2 ) << "ChildBackProp::independent " << job;

  if ( !dependees_.empty() ) {
    auto dependee = dependees_.at( job );

    unordered_set<shared_ptr<IRuntime>> move_output_to;
    for ( const auto& d : dependee ) {
      move_output_to.insert( chosen_remotes_.at( d ).first );
    }

    optional<shared_ptr<IRuntime>> chosen_remote;

    if ( move_output_to.contains( chosen_remotes_.at( job ).first ) ) {
      return;
    }

    job.visit<void>( overload { [&]( auto r ) {
                                 const auto& contains = base_.get().get_contains( r );
                                 if ( !contains.empty() ) {
                                   for ( const auto& c : contains ) {
                                     if ( move_output_to.contains( c ) ) {
                                       chosen_remote = c;
                                       return;
                                     }
                                   }
                                   chosen_remote = chosen_remotes_.at( r ).first;
                                 }
                               },
                                []( Handle<Literal> ) {} } );

    if ( !chosen_remote.has_value() ) {
      // Previous score for selecting the current assignment
      auto prev = chosen_remotes_.at( job ).second;

      const auto& absent_size = base_.get().get_absent_size( job );

      int64_t min_absent_size = numeric_limits<int64_t>::max();
      size_t max_parallelism = 0;
      int64_t min_absent_size_diff = 0;

      for ( const auto& [r, s] : absent_size ) {
        if ( r == chosen_remotes_.at( job ).first ) {
          continue;
        }

        int64_t sum_size = s - ( move_output_to.contains( r ) ? base_.get().get_output_size( job ) : 0 );
        if ( sum_size < prev ) {
          if ( sum_size < min_absent_size ) {
            min_absent_size_diff = sum_size - min_absent_size;
            min_absent_size = sum_size;
            max_parallelism = r->get_info()->parallelism;
            chosen_remote = r;
          } else if ( sum_size == min_absent_size && r->get_info()->parallelism > max_parallelism ) {
            max_parallelism = r->get_info()->parallelism;
            chosen_remote = r;
          } else if ( sum_size == min_absent_size && r->get_info()->parallelism == max_parallelism
                      && is_local( r ) ) {
            chosen_remote = r;
          }
        }
      }

      if ( chosen_remote.has_value() ) {
        VLOG( 2 ) << "ChildBackProp::independent move " << job << " to "
                  << ( is_local( chosen_remote.value() ) ? "local" : "remote" );
        chosen_remotes_.insert_or_assign( job, { chosen_remote.value(), min_absent_size_diff } );
      }
    } else {
      VLOG( 2 ) << "ChildBackProp::independent move " << job << " to "
                << ( is_local( chosen_remote.value() ) ? "local" : "remote" );
      chosen_remotes_.insert_or_assign( job, { chosen_remote.value(), 0 } );
    }
  }
}

void ChildBackProp::pre( Handle<AnyDataType> job, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  for ( const auto& d : dependencies ) {
    dependees_[d].insert( job );
  }
}

void OutSource::pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  size_t assigned = 0;
  multimap<int64_t, Handle<AnyDataType>, greater<int64_t>> scores;
  optional<shared_ptr<IRuntime>> local;

  for ( auto d : dependencies ) {
    if ( is_local( chosen_remotes_.at( d ).first ) ) {
      if ( !local.has_value() ) {
        local = chosen_remotes_.at( d ).first;
      }
      assigned += base_.get().get_fan_out( d );
      scores.insert( { chosen_remotes_.at( d ).second, d } );
    }
  }

  if ( local.has_value() and assigned > local.value()->get_info()->parallelism ) {
    unordered_map<shared_ptr<IRuntime>, size_t> assigned_tasks;
    for ( const auto& [_, d] : scores ) {
      // XXX: We may want to oursource less
      if ( assigned - base_.get().get_fan_out( d ) < local.value()->get_info()->parallelism ) {
        break;
      }

      // Choose a different remote for this dependency
      // XXX: calculate score by queue length and data movement cost
      const auto& absent_size = base_.get().get_absent_size( d );

      size_t min_absent_size = numeric_limits<size_t>::max();

      optional<shared_ptr<IRuntime>> chosen_remote;

      for ( const auto& [r, s] : absent_size ) {
        if ( is_local( r ) )
          continue;

        if ( s < min_absent_size ) {
          min_absent_size = s;
          chosen_remote = r;
        } else if ( s == min_absent_size && chosen_remote.has_value()
                    && assigned_tasks[r] < assigned_tasks[chosen_remote.value()] ) {
          chosen_remote = r;
        }
      }

      if ( chosen_remote.has_value() ) {
        VLOG( 2 ) << "Outsource moved " << d << ( is_local( chosen_remote.value() ) ? " locally" : " remotely" );
        chosen_remotes_.insert_or_assign( d, { chosen_remote.value(), min_absent_size } );
        assigned -= base_.get().get_fan_out( d );
        assigned_tasks[chosen_remote.value()] += base_.get().get_fan_out( d );
      } else {
        // No outsourcable locatons
        break;
      }
    }
  }
}

void RandomSelection::independent( Handle<AnyDataType> job )
{
  // XXX
  const auto& absent_size = base_.get().get_absent_size( job );
  size_t rand_idx = rand() % absent_size.size();

  size_t i = 0;
  for ( const auto& [r, _] : absent_size ) {
    if ( i == rand_idx ) {
      chosen_remotes_.insert_or_assign( job, { r, 0 } );
      break;
    }

    i++;
  }
}

void FinalPass::leaf( Handle<AnyDataType> job )
{
  VLOG( 1 ) << "Run job " << ( is_local( chosen_remotes_.at( job ).first ) ? "locally " : "remotely " ) << job
            << endl;
  job.visit<void>(
    overload { []( Handle<Literal> ) {}, [&]( auto h ) { chosen_remotes_.at( job ).first->get( h ); } } );
}

void FinalPass::independent( Handle<AnyDataType> job )
{
  if ( !is_local( chosen_remotes_.at( job ).first ) ) {
    VLOG( 1 ) << "Run job remotely " << job << endl;
    job.visit<void>(
      overload { []( Handle<Literal> ) {}, [&]( auto h ) { chosen_remotes_.at( job ).first->get( h ); } } );
  }
}

void FinalPass::pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  for ( auto d : dependencies ) {
    if ( !is_local( chosen_remotes_.at( d ).first ) ) {
      VLOG( 1 ) << "Run job remotely " << d << endl;
      d.visit<void>(
        overload { []( Handle<Literal> ) {}, [&]( auto h ) { chosen_remotes_.at( d ).first->get( h ); } } );
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
          selection = make_unique<MinAbsentMaxParallelism>( base, rt, move( selection.value() ) );
        } else {
          selection = make_unique<MinAbsentMaxParallelism>( base, rt );
        }
        selection.value()->run( top_level_job );
        break;
      }

      case PassType::ChildBackProp: {
        if ( selection.has_value() ) {
          selection = make_unique<ChildBackProp>( base, rt, move( selection.value() ) );
        } else {
          selection = make_unique<ChildBackProp>( base, rt );
        }
        selection.value()->run( top_level_job );
        break;
      }

      case PassType::OutSource: {
        if ( selection.has_value() ) {
          selection = make_unique<OutSource>( base, rt, move( selection.value() ) );
        } else {
          throw runtime_error( "Invalid pass sequence." );
        }
        selection.value()->run( top_level_job );
        break;
      }

      case PassType::Random: {
        selection = make_unique<RandomSelection>( base, rt );
        selection.value()->run( top_level_job );
        break;
      }
    }
  }

  if ( not selection.has_value() ) {
    throw runtime_error( "Invalid pass sequence." );
  }

  FinalPass final( base, rt, move( selection.value() ) );
  final.run( top_level_job );
}
