#include "pass.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "handle_util.hh"
#include "overload.hh"
#include "relater.hh"
#include <limits>
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
                                   independent( e );
                                   pre( e, sketch_graph_.get_forward_dependencies( e ) );
                                   for ( const auto& dependency : sketch_graph_.get_forward_dependencies( e ) ) {
                                     run( dependency );
                                   }
                                   post( e, sketch_graph_.get_forward_dependencies( e ) );
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
  if ( !chosen_remotes_.contains( job ) ) {
    throw std::runtime_error( "Chosen remotes has no info about job" );
  }

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
                                   independent( e );
                                   pre( e, sketch_graph_.get_forward_dependencies( e ) );
                                   for ( const auto& dependency : sketch_graph_.get_forward_dependencies( e ) ) {
                                     if ( is_local( chosen_remotes_.at( dependency ).first ) ) {
                                       run( dependency );
                                     }
                                   }
                                   post( e, sketch_graph_.get_forward_dependencies( e ) );
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
                                   bool ep = ( limits->size() > 3 )
                                               ? handle::extract<Literal>( limits->at( 3 ) )
                                                   .transform( [&]( auto x ) { return uint8_t( x ); } )
                                                   .value_or( 0 )
                                               : false;

                                   tasks_info_[job].output_size = output_size;
                                   tasks_info_[job].output_fan_out = output_fan_out;
                                   tasks_info_[job].ep = ep;
                                 },
                                 [&]( Handle<Eval> ) {},
                               } );
                             },
                              []( Handle<Literal> ) { throw std::runtime_error( "Unreachable" ); },
                              [&]( auto h ) {
                                tasks_info_[job].output_size = handle::byte_size( h );
                                tasks_info_[job].output_fan_out = 0;
                              } } );
}

void BasePass::post( Handle<Eval> job, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  // Output size = input_size + replacing every dependent's input with output
  Handle<Object> obj = job.unwrap<Object>();
  auto input_size = handle::byte_size( obj );

  size_t output_size = input_size;
  size_t output_fan_out = 0;
  for ( const auto& dependency : dependencies ) {
    if ( !tasks_info_.contains( dependency ) ) {
      throw std::runtime_error( "Task info does not contain dependency" );
    }
    output_fan_out += tasks_info_.at( dependency ).output_fan_out;
    auto out = tasks_info_.at( dependency ).output_size;
    output_size += out;
  }

  auto outs = obj.visit<tuple<size_t, size_t, bool>>( overload {
    [&]( Handle<Thunk> t ) {
      return t.visit<tuple<size_t, size_t, bool>>( overload {
        [&]( Handle<Application> a ) -> tuple<size_t, size_t, bool> {
          if ( relater_.get().contains( a.unwrap<ExpressionTree>() ) ) {
            auto rlimits = relater_.get().get( a.unwrap<ExpressionTree>() ).value()->at( 0 );
            auto limits
              = handle::extract<ValueTree>( rlimits ).and_then( [&]( auto x ) { return relater_.get().get( x ); } );

            auto curr_output_size
              = limits.and_then( [&]( auto x ) { return handle::extract<Literal>( x->at( 1 ) ); } )
                  .transform( [&]( auto x ) { return uint64_t( x ); } )
                  .value_or( output_size );
            auto curr_fan_out
              = max( limits.and_then( [&]( auto x ) { return handle::extract<Literal>( x->at( 2 ) ); } )
                       .transform( [&]( auto x ) { return uint64_t( x ); } )
                       .value_or( output_fan_out ),
                     output_fan_out );
            bool ep = limits
                        .transform( [&]( auto x ) -> bool {
                          return ( x->size() > 3 ) ? handle::extract<Literal>( x->at( 3 ) )
                                                       .transform( [&]( auto x ) { return uint8_t( x ); } )
                                                       .value_or( 0 )
                                                   : false;
                        } )
                        .value_or( false );

            if ( ep ) {
              if ( ( output_size - input_size ) > 20 * curr_output_size ) {
                ep = false;
              }
            }

            return { curr_output_size, curr_fan_out, ep };
          } else {
            return { output_size, output_fan_out, false };
          }
        },
        [&]( Handle<Identification> ) -> tuple<size_t, size_t, bool> {
          if ( dependencies.size() == 0 ) {
            VLOG( 3 ) << "Dependency preresolved";
            return { output_size, 1, false };
          }
          if ( !tasks_info_.contains( *dependencies.begin() ) ) {
            throw std::runtime_error( "Task info does not contain dependency" );
          }
          auto out = tasks_info_.at( *dependencies.begin() ).output_size;
          return { out, 1, false };
        },
        [&]( Handle<Selection> ) -> tuple<size_t, size_t, bool> {
          throw std::runtime_error( "Unimplemented" );
        } } );
    },
    [&]( auto ) -> tuple<size_t, size_t, bool> {
      return { output_size, output_fan_out, false };
    } } );

  tasks_info_[job].output_size = get<0>( outs );
  tasks_info_[job].output_fan_out = get<1>( outs );
  tasks_info_[job].ep = get<2>( outs );
}

size_t BasePass::absent_size( std::shared_ptr<IRuntime> worker, Handle<AnyDataType> job )
{
  auto root = job::get_root( job );

  size_t contained_size = 0;
  relater_.get().early_stop_visit_minrepo( root, [&]( Handle<AnyDataType> handle ) -> bool {
    auto contained = handle.visit<bool>(
      overload { [&]( Handle<Literal> ) { return true; }, [&]( auto h ) { return worker->contains( h ); } } );
    if ( contained ) {
      contained_size += handle::byte_size( handle );
    }

    return contained;
  } );
  return handle::byte_size( root ) - contained_size;
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
      VLOG( 2 ) << "remote absent_size " << job << " " << tasks_info_[job].absent_size.at( locked_remote )
                << locked_remote;
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
    optional<size_t> min_absent_size;
    optional<int64_t> min_absent_size_diff;

    for ( const auto& [r, s] : absent_size ) {
      if ( !min_absent_size.has_value() ) {
        min_absent_size = s;
        max_parallelism = r->get_info()->parallelism;
        chosen_remote = r;
      } else {
        if ( !min_absent_size_diff.has_value() ) {
          min_absent_size_diff = min_absent_size.value() - s;
        }

        if ( s < min_absent_size.value() ) {
          min_absent_size_diff = s - min_absent_size.value();
          min_absent_size = s;
          max_parallelism = r->get_info()->parallelism;
          chosen_remote = r;
        } else if ( s == min_absent_size.value() && r->get_info()->parallelism > max_parallelism ) {
          min_absent_size_diff = 0;
          max_parallelism = r->get_info()->parallelism;
          chosen_remote = r;
        } else if ( s == min_absent_size.value() && r->get_info()->parallelism == max_parallelism
                    && is_local( r ) ) {
          min_absent_size_diff = 0;
          chosen_remote = r;
        }
      }
    }

    VLOG( 2 ) << "MinAbsent::leaf " << job << " " << chosen_remote.value();
    chosen_remotes_.insert_or_assign( job, { chosen_remote.value(), min_absent_size_diff.value_or( 0 ) } );
  } else {
    VLOG( 2 ) << "MinAbsent::leaf " << job << " " << chosen_remote.value();
    chosen_remotes_.insert_or_assign( job, { chosen_remote.value(), 0 } );
  }
}

void MinAbsentMaxParallelism::post( Handle<Eval> job, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  optional<shared_ptr<IRuntime>> chosen_remote;

  if ( base_.get().get_ep( job ) ) {
    chosen_remote = relater_.get().get_local();
  }

  if ( !chosen_remote.has_value() ) {
    const auto& contains = base_.get().get_contains( job );
    if ( !contains.empty() ) {
      chosen_remote = *contains.begin();
    }
  }

  if ( !chosen_remote.has_value() ) {
    absl::flat_hash_map<shared_ptr<IRuntime>, size_t> present_output;

    for ( auto d : dependencies ) {
      if ( !chosen_remotes_.contains( d ) ) {
        throw std::runtime_error( "chosen_remotes_ does not contain d " );
      }
      present_output[chosen_remotes_.at( d ).first] += base_.get().get_output_size( d );

      d.visit<void>( overload { [&]( Handle<Relation> r ) {
                                 r.visit<void>( overload { [&]( Handle<Eval> ) {
                                                            present_output[chosen_remotes_.at( d ).first]
                                                              += sizeof( Handle<Fix> );
                                                          },
                                                           []( Handle<Apply> ) {} } );
                               },
                                []( auto ) {} } );
      // TODO: handle data and load differently
    }

    const auto& absent_size = base_.get().get_absent_size( job );

    optional<int64_t> min_absent_size;
    size_t max_parallelism = 0;
    optional<int64_t> min_absent_size_diff;

    for ( const auto& [r, s] : absent_size ) {
      int64_t sum_size = s - present_output[r];
      if ( !min_absent_size.has_value() ) {
        min_absent_size = sum_size;
        max_parallelism = r->get_info()->parallelism;
        chosen_remote = r;
      } else {
        if ( !min_absent_size_diff.has_value() ) {
          min_absent_size_diff = min_absent_size.value() - s;
        }

        if ( sum_size < min_absent_size.value() ) {
          min_absent_size_diff = sum_size - min_absent_size.value();
          min_absent_size = sum_size;
          max_parallelism = r->get_info()->parallelism;
          chosen_remote = r;
        } else if ( sum_size == min_absent_size.value() && r->get_info()->parallelism > max_parallelism ) {
          max_parallelism = r->get_info()->parallelism;
          chosen_remote = r;
        } else if ( sum_size == min_absent_size.value() && r->get_info()->parallelism == max_parallelism
                    && is_local( r ) ) {
          chosen_remote = r;
        }
      }
    }
    VLOG( 2 ) << "MinAbsent::post " << job << " " << chosen_remote.value();
    chosen_remotes_.insert_or_assign( job, { chosen_remote.value(), min_absent_size_diff.value_or( 0 ) } );
  } else {
    VLOG( 2 ) << "MinAbsent::post " << job << " " << chosen_remote.value();
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
    int64_t score = 0;
    auto curr_output_size = base_.get().get_output_size( job );

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
                                   score = -curr_output_size;
                                 }
                               },
                                []( Handle<Literal> ) {} } );

    if ( !chosen_remote.has_value() ) {
      // How much current choice is better than other choices (the smaller the better)
      auto prev = chosen_remotes_.at( job ).second;

      if ( prev + static_cast<int64_t>( curr_output_size ) <= 0 ) {
        return;
      }

      score = 0 - ( prev + curr_output_size );
      // Pick the one with the smallest absent_size
      if ( move_output_to.size() == 1 ) {
        chosen_remote = *move_output_to.begin();
      } else {
        optional<size_t> min_absent_size;
        int64_t max_parallelism = 0;
        const auto& absent_size = base_.get().get_absent_size( job );

        for ( const auto& r : move_output_to ) {
          auto s = absent_size.at( r );
          if ( !min_absent_size.has_value() ) {
            min_absent_size = s;
            max_parallelism = r->get_info()->parallelism;
            chosen_remote = r;
          } else {
            if ( s < min_absent_size.value() ) {
              min_absent_size = s;
              max_parallelism = r->get_info()->parallelism;
              chosen_remote = r;
            } else if ( s == min_absent_size.value() && r->get_info()->parallelism > max_parallelism ) {
              max_parallelism = r->get_info()->parallelism;
              chosen_remote = r;
            } else if ( s == min_absent_size.value() && r->get_info()->parallelism == max_parallelism
                        && is_local( r ) ) {
              chosen_remote = r;
            }
          }
        }
      }
    }

    if ( chosen_remote.has_value() ) {
      VLOG( 2 ) << "ChildBackProp::independent move " << job << " to "
                << ( is_local( chosen_remote.value() ) ? "local" : "remote" );
      chosen_remotes_.insert_or_assign( job, { chosen_remote.value(), score } );
    }
  }
}

void ChildBackProp::pre( Handle<Eval> job, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  for ( const auto& d : dependencies ) {
    dependees_[d].insert( Handle<AnyDataType>( Handle<Relation>( job ) ) );
  }
}

void InOutSource::pre( Handle<Eval> job, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  bool any_ep = false;
  for ( auto d : dependencies ) {
    if ( base_.get().get_ep( d ) ) {
      chosen_remotes_.at( d ) = { relater_.get().get_local(), 0 };
      any_ep = true;
    }
  }
  if ( any_ep )
    return;

  auto prev = chosen_remotes_.at( job ).second;
  if ( prev + dependencies.size() * sizeof( Handle<Fix> ) == 0 ) {
    optional<shared_ptr<IRuntime>> remote;
    for ( auto d : dependencies ) {
      if ( !remote.has_value() ) {
        remote = chosen_remotes_.at( d ).first;
      } else {
        if ( remote.value() != chosen_remotes_.at( d ).first ) {
          remote.reset();
          break;
        }
      }
    }

    if ( remote.has_value() && !is_local( remote.value() ) ) {
      chosen_remotes_.insert_or_assign( job, { remote.value(), 0 } );
      sketch_graph_.erase_forward_dependencies( job );
      return;
    }
  }

  size_t assigned = 0;
  multimap<int64_t, Handle<AnyDataType>, greater<int64_t>> scores;
  optional<shared_ptr<IRuntime>> local;

  for ( auto d : dependencies ) {
    if ( is_local( chosen_remotes_.at( d ).first ) ) {
      if ( !local.has_value() ) {
        local = chosen_remotes_.at( d ).first;
      }
      assigned += 1;
      scores.insert( { chosen_remotes_.at( d ).second, d } );
    }
  }

  if ( local.has_value() and assigned > local.value()->get_info()->parallelism ) {
    // Collect available remotes
    std::vector<shared_ptr<IRuntime>> available_remotes;
    for ( auto d : dependencies ) {
      const auto& absent_size = base_.get().get_absent_size( d );
      for ( const auto& [r, _] : absent_size ) {
        if ( is_local( r ) )
          continue;

        available_remotes.push_back( r );
      }
      break;
    }

    size_t local_jobs = std::thread::hardware_concurrency();

    size_t remote_idx = 0;
    for ( const auto& [_, d] : scores ) {
      if ( assigned - 1 < local_jobs ) {
        break;
      }

      chosen_remotes_.insert_or_assign( d, { available_remotes[remote_idx], 0 } );

      assigned--;
      remote_idx++;

      if ( remote_idx == available_remotes.size() ) {
        remote_idx = 0;
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

void SendToRemotePass::independent( Handle<AnyDataType> job )
{
  if ( !is_local( chosen_remotes_.at( job ).first ) ) {
    VLOG( 1 ) << "Run job remotely " << job << endl;
    remote_jobs_[chosen_remotes_.at( job ).first].insert( job );
  }
}

void SendToRemotePass::pre( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& dependencies )
{
  for ( auto d : dependencies ) {
    if ( !is_local( chosen_remotes_.at( d ).first ) ) {
      VLOG( 1 ) << "Run job remotely " << d << endl;
      remote_jobs_[chosen_remotes_.at( d ).first].insert( d );
    }
  }
}

void SendToRemotePass::send_job_dependencies( shared_ptr<IRuntime> rt, Handle<AnyDataType> job )
{
  auto remote_contained = job.visit<bool>(
    overload { []( Handle<Literal> ) { return true; }, [&]( auto x ) { return rt->contains( x ); } } );

  if ( remote_contained ) {
    return;
  }

  job.visit<void>( overload { []( Handle<Literal> ) {},
                              [&]( Handle<Relation> r ) {
                                r.visit<void>( overload { [&]( Handle<Apply> a ) {
                                                           if ( relater_.get().contains( a ) ) {
                                                             remote_data_[rt].insert( a );
                                                           }
                                                         },
                                                          [&]( Handle<Eval> e ) {
                                                            if ( relater_.get().contains( e ) ) {
                                                              remote_data_[rt].insert( e );
                                                            } else {
                                                              auto data = handle::data( e.unwrap<Object>() );
                                                              if ( data.has_value() ) {
                                                                remote_data_[rt].insert( data.value() );
                                                              }

                                                              for ( auto d :
                                                                    sketch_graph_.get_forward_dependencies( e ) ) {
                                                                send_job_dependencies( rt, d );
                                                              }
                                                            }
                                                          } } );
                              },
                              [&]( auto d ) {
                                if ( relater_.get().contains( d ) ) {
                                  remote_data_[rt].insert( d );
                                }
                              } } );
}

void SendToRemotePass::send_remote_jobs( Handle<AnyDataType> root )
{
  this->run( root );

  vector<pair<shared_ptr<IRuntime>, absl::flat_hash_set<Handle<AnyDataType>>::const_iterator>> iterators {};
  for ( const auto& [rt, handles] : remote_jobs_ ) {
    iterators.push_back( make_pair( rt, handles.begin() ) );
    for ( auto h : handles ) {
      send_job_dependencies( rt, h );
    }
  }

  for ( const auto& [rt, data] : remote_data_ ) {
    for ( auto d : data ) {
      d.visit<void>( overload { []( Handle<Literal> ) {},
                                [&]( Handle<Relation> r ) { rt->put_force( r, relater_.get().get( r ).value() ); },
                                [&]( auto x ) { rt->put( x, relater_.get().get( x ).value() ); } } );
    }
  }

  unordered_set<shared_ptr<IRuntime>> finished;
  size_t iterator_index = 0;
  while ( finished.size() < iterators.size() ) {
    auto& [r, it] = iterators.at( iterator_index );
    if ( it != remote_jobs_.at( r ).end() ) {
      auto h = *it;
      h.visit<void>( overload { []( Handle<Literal> ) {},
                                [&]( Handle<Relation> d ) {
                                  r->get( d );
                                  sketch_graph_.erase_forward_dependencies( d );
                                },
                                [&]( auto d ) { r->get( d ); } } );
      it++;
    } else {
      finished.insert( r );
    }

    iterator_index++;
    if ( iterator_index == iterators.size() ) {
      iterator_index = 0;
    }
  }
}

void FinalPass::leaf( Handle<AnyDataType> job )
{
  VLOG( 1 ) << "Run job " << ( is_local( chosen_remotes_.at( job ).first ) ? "locally " : "remotely " ) << job
            << endl;
  if ( is_local( chosen_remotes_.at( job ).first ) ) {
    job.visit<void>(
      overload { []( Handle<Literal> ) {}, [&]( auto h ) { chosen_remotes_.at( job ).first->get( h ); } } );
  }
}

void FinalPass::pre( Handle<Eval> job, const absl::flat_hash_set<Handle<AnyDataType>>& )
{
  if ( relater_.get().contains( job ) ) {
    sketch_graph_.erase_forward_dependencies( job );
  }

  absl::flat_hash_set<Handle<Relation>> unblocked;
  relater_.get().merge_sketch_graph( job, unblocked );

  for ( auto r : unblocked ) {
    if ( is_local( chosen_remotes_.at( r ).first ) ) {
      chosen_remotes_.at( r ).first->get( r );
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

      case PassType::InOutSource: {
        if ( selection.has_value() ) {
          selection = make_unique<InOutSource>( base, rt, move( selection.value() ) );
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

  selection = make_unique<SendToRemotePass>( base, rt, move( selection.value() ) );
  dynamic_cast<SendToRemotePass*>( selection.value().get() )->send_remote_jobs( top_level_job );
  FinalPass final( base, rt, move( selection.value() ) );
  final.run( top_level_job );
}
