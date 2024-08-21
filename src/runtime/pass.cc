#include "pass.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "handle_util.hh"
#include "overload.hh"
#include "relater.hh"
#include "scheduler.hh"
#include <functional>
#include <limits>
#include <ostream>
#include <stdexcept>

using namespace std;

bool is_local( shared_ptr<IRuntime> rt )
{
  return rt->get_info()->link_speed == numeric_limits<double>::max();
}

Pass::Pass( reference_wrapper<Relater> relater )
  : relater_( relater )
  , local_( relater.get().get_local() )
{}

void Pass::run( Handle<Dependee> job )
{
  job.visit<void>( overload {
    [&]( Handle<Relation> r ) {
      all( r );
      relation_pre( r, sketch_graph_.get_forward_dependencies( r ) );
      for ( const auto& dependency : sketch_graph_.get_forward_dependencies( r ) ) {
        run( dependency );
      }
      relation_post( r, sketch_graph_.get_forward_dependencies( r ) );
    },
    [&]( auto h ) {
      all( h );
      data( h );
    },
  } );
}

void PrunedSelectionPass::run( Handle<Dependee> job )
{
  if ( !chosen_remotes_.contains( job ) ) {
    throw std::runtime_error( "Chosen remotes has no info about job" );
  }

  if ( !is_local( chosen_remotes_.at( job ).first ) ) {
    all( job );
    return;
  }

  job.visit<void>( overload {
    [&]( Handle<Relation> r ) {
      all( r );
      relation_pre( r, sketch_graph_.get_forward_dependencies( r ) );
      for ( const auto& dependency : sketch_graph_.get_forward_dependencies( r ) ) {
        if ( chosen_remotes_.contains( dependency ) and is_local( chosen_remotes_.at( dependency ).first ) ) {
          run( dependency );
        }
      }
      relation_post( r, sketch_graph_.get_forward_dependencies( r ) );
    },
    [&]( auto h ) {
      all( h );
      data( h );
    },
  } );
}

BasePass::BasePass( reference_wrapper<Relater> relater )
  : Pass( relater )
{}

void BasePass::data( Handle<Dependee> job )
{
  job.visit<void>( overload {
    []( Handle<Relation> ) { throw std::runtime_error( "Unreachable" ); },
    [&]( Handle<ValueTreeRef> ref ) {
      tasks_info_[job].output_size = ref.size() * sizeof( Handle<Fix> );
      tasks_info_[job].output_fan_out = 0;
      tasks_info_[job].ep = false;
    },
    [&]( Handle<ObjectTreeRef> ref ) {
      tasks_info_[job].output_size = ref.size() * sizeof( Handle<Fix> );
      tasks_info_[job].output_fan_out = 0;
      tasks_info_[job].ep = false;
    },
    [&]( auto h ) {
      tasks_info_[job].output_size = handle::byte_size( h );
      tasks_info_[job].output_fan_out = 0;
      tasks_info_[job].ep = false;
    },
  } );
}

void BasePass::relation_post( Handle<Relation> job, const absl::flat_hash_set<Handle<Dependee>>& dependencies )
{
  if ( dependencies.empty() ) {
    if ( !relater_.get().contains( job ) ) {
      throw std::runtime_error( "Unreachable" );
    }
    tasks_info_[job].output_size = handle::byte_size( relater_.get().get( job ).value() );
    tasks_info_[job].output_fan_out = 0;
    tasks_info_[job].ep = false;

    return;
  }

  job.visit<void>( overload {
    [&]( Handle<Step> s ) {
      s.unwrap<Thunk>().visit<void>( overload {
        [&]( Handle<Application> a ) {
          if ( relater_.get().contains( a.unwrap<ExpressionTree>() ) ) {
            auto rlimits = relater_.get().get( a.unwrap<ExpressionTree>() ).value()->at( 0 );
            auto limits
              = handle::extract<ValueTree>( rlimits ).and_then( [&]( auto x ) { return relater_.get().get( x ); } );

            auto curr_output_size
              = limits.and_then( [&]( auto x ) { return handle::extract<Literal>( x->at( 1 ) ); } )
                  .transform( [&]( auto x ) { return uint64_t( x ); } )
                  .value_or( 1 );

            auto curr_fan_out
              = max( limits.and_then( [&]( auto x ) { return handle::extract<Literal>( x->at( 2 ) ); } )
                       .transform( [&]( auto x ) { return uint64_t( x ); } )
                       .value_or( 1 ),
                     // XXX
                     dependencies.size() );
            bool ep = limits
                        .transform( [&]( auto x ) -> bool {
                          return ( x->size() > 3 ) ? handle::extract<Literal>( x->at( 3 ) )
                                                       .transform( [&]( auto x ) { return uint8_t( x ); } )
                                                       .value_or( 0 )
                                                   : false;
                        } )
                        .value_or( false );

            tasks_info_[job].output_size = curr_output_size;
            tasks_info_[job].output_fan_out = curr_fan_out;

            if ( ep ) {
              size_t input_size = 0;
              for ( const auto& dependency : dependencies ) {
                if ( !tasks_info_.contains( dependency ) ) {
                  throw std::runtime_error( "Task info does not contain dependency" );
                }
                input_size += tasks_info_.at( dependency ).output_size;
              }

              if ( input_size / curr_output_size > 20 ) {
                ep = false;
              }
            }

            tasks_info_[job].ep = ep;

          } else {
            tasks_info_[job].output_size = 1;
            tasks_info_[job].output_fan_out = 1;
            tasks_info_[job].ep = false;
          }
        },
        [&]( Handle<Identification> ) {
          if ( !tasks_info_.contains( *dependencies.begin() ) ) {
            throw std::runtime_error( "Task info does not contain dependency" );
          }

          tasks_info_[job].output_size = handle::byte_size( *dependencies.begin() );
          tasks_info_[job].output_fan_out = 0;
          tasks_info_[job].ep = false;
          // return { out, 1, false };
        },
        [&]( Handle<Selection> s ) {
          if ( relater_.get().contains_shallow( s.unwrap<ObjectTree>() ) ) {
            auto d = relater_.get().get_shallow( s.unwrap<ObjectTree>() ).value();

            if ( d->span().size() == 2 ) {
              tasks_info_[job].output_size = sizeof( Handle<Fix> );
            } else {
              auto begin_idx = size_t(
                d->at( 1 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
              auto end_idx = size_t(
                d->at( 2 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
              tasks_info_[job].output_size = ( end_idx - begin_idx ) * sizeof( Handle<Fix> );
            }
            tasks_info_[job].output_fan_out = 0;
            tasks_info_[job].ep = false;

            for ( auto dependency : dependencies ) {
              bool check = dependency.visit<bool>( overload {
                []( Handle<BlobRef> ) { return true; },
                [&]( Handle<ValueTreeRef> t ) {
                  return !handle::any_tree_equal()( Handle<AnyTree>::forge( t.content ), s.unwrap<ObjectTree>() );
                },
                [&]( Handle<ObjectTreeRef> t ) {
                  return !handle::any_tree_equal()( Handle<AnyTree>::forge( t.content ), s.unwrap<ObjectTree>() );
                },
                []( auto ) { return false; } } );

              if ( check && tasks_info_.at( dependency ).output_size / tasks_info_.at( job ).output_size < 20 ) {
                tasks_info_[job].ep = true;
              }
            }
          } else {
            tasks_info_[job].output_size = sizeof( Handle<Fix> );
            tasks_info_[job].output_fan_out = 0;
            tasks_info_[job].ep = false;
          }
        },
      } );
    },
    [&]( Handle<Eval> e ) {
      Handle<Object> obj = e.unwrap<Object>();
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

      tasks_info_[job].output_size = output_size;
      tasks_info_[job].output_fan_out = output_fan_out;
    },
  } );
}

size_t BasePass::absent_size( std::shared_ptr<IRuntime> worker, Handle<Dependee> job )
{
  auto root = job::get_root( job );

  size_t absent = job.visit<size_t>(
    overload { [&]( Handle<AnyTreeRef> r ) -> size_t {
                relater_.get().unref( r );
                if ( worker->contains_shallow( relater_.get().unref( r ) ) ) {
                  return 0;
                } else {
                  return r.visit<size_t>( []( auto x ) { return x.size(); } ) * sizeof( Handle<Fix> );
                }
              },
               [&]( auto ) {
                 size_t contained_size = 0;
                 relater_.get().early_stop_visit_minrepo( root, [&]( Handle<AnyDataType> handle ) -> bool {
                   auto contained = handle.visit<bool>( overload {
                     []( Handle<Literal> ) { return true; },
                     [&]( auto h ) { return worker->contains( h ); },
                   } );
                   if ( contained ) {
                     contained_size += handle::byte_size( handle );
                   }

                   return contained;
                 } );
                 return handle::byte_size( root ) - contained_size;
               } } );

  return absent;
}

void BasePass::all( Handle<Dependee> job )
{
  {
    auto local = relater_.get().local_;
    if ( local->get_info().has_value() and local->get_info()->parallelism > 0 ) {
      tasks_info_[job].absent_size.insert( { local, absent_size( local, job ) } );
      VLOG( 2 ) << "local absent_size " << job << " " << tasks_info_[job].absent_size.at( local );

      job.visit<void>( overload {
        [&]( Handle<ValueTreeRef> ref ) {
          if ( local->contains_shallow( relater_.get().unref( ref ) ) ) {
            tasks_info_[job].contains.insert( local );
          }
        },
        [&]( Handle<ObjectTreeRef> ref ) {
          if ( local->contains_shallow( relater_.get().unref( ref ) ) ) {
            tasks_info_[job].contains.insert( local );
          }
        },
        [&]( auto r ) {
          if ( local->contains( r ) ) {
            tasks_info_[job].contains.insert( local );
          }
        },
      } );
    }
  }

  for ( const auto& remote : relater_.get().remotes_.read().get() ) {
    auto locked_remote = remote.lock();
    if ( locked_remote->get_info().has_value() and locked_remote->get_info()->parallelism > 0 ) {
      tasks_info_[job].absent_size.insert( { locked_remote, absent_size( locked_remote, job ) } );
      VLOG( 2 ) << "remote absent_size " << job << " " << tasks_info_[job].absent_size.at( locked_remote )
                << locked_remote;
      job.visit<void>( overload {
        [&]( Handle<ValueTreeRef> ref ) {
          if ( locked_remote->contains_shallow( relater_.get().unref( ref ) ) ) {
            tasks_info_[job].contains.insert( locked_remote );
          }
        },
        [&]( Handle<ObjectTreeRef> ref ) {
          if ( locked_remote->contains_shallow( relater_.get().unref( ref ) ) ) {
            tasks_info_[job].contains.insert( locked_remote );
          }
        },
        [&]( auto r ) {
          if ( locked_remote->contains( r ) ) {
            tasks_info_[job].contains.insert( locked_remote );
          }
        },
      } );
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

void MinAbsentMaxParallelism::relation_post( Handle<Relation> job,
                                             const absl::flat_hash_set<Handle<Dependee>>& dependencies )
{
  if ( dependencies.empty() )
    return;

  optional<shared_ptr<IRuntime>> chosen_remote;

  const auto& contains = base_.get().get_contains( job );

  if ( !contains.empty() ) {
    chosen_remote = *contains.begin();
  }

  if ( base_.get().get_ep( job ) ) {
    chosen_remote = local_;
  }

  if ( !chosen_remote.has_value() ) {
    absl::flat_hash_map<shared_ptr<IRuntime>, size_t> present_output;
    for ( auto d : dependencies ) {

      if ( !chosen_remotes_.contains( d ) ) {
        const auto& contains = base_.get().get_contains( d );
        for ( const auto& s : contains ) {
          present_output[s] += base_.get().get_output_size( d );
        }
      } else {
        present_output[chosen_remotes_.at( d ).first] += base_.get().get_output_size( d );
      }
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

void ChildBackProp::run( Handle<Dependee> job )
{
  job.visit<void>( overload {
    [&]( Handle<Relation> r ) {
      all( r );
      relation_pre( r, sketch_graph_.get_forward_dependencies( r ) );
      if ( !is_local( chosen_remotes_.at( job ).first ) )
        return;

      for ( const auto& dependency : sketch_graph_.get_forward_dependencies( r ) ) {
        run( dependency );
      }

      relation_post( r, sketch_graph_.get_forward_dependencies( r ) );
    },
    [&]( auto d ) { data( d ); },
  } );
}

void ChildBackProp::all( Handle<Dependee> job )
{
  VLOG( 2 ) << "ChildBackProp::independent " << job;

  if ( base_.get().get_ep( job ) ) {
    return;
  }

  if ( !dependees_.empty() ) {
    auto dependee = dependees_.at( job );

    unordered_set<shared_ptr<IRuntime>> move_output_to;
    for ( const auto& d : dependee ) {
      move_output_to.insert( chosen_remotes_.at( d ).first );
    }

    optional<shared_ptr<IRuntime>> chosen_remote;
    int64_t score = 0;
    auto curr_output_size = base_.get().get_output_size( job );

    if ( chosen_remotes_.contains( job ) and move_output_to.contains( chosen_remotes_.at( job ).first ) ) {
      return;
    }

    job.visit<void>( overload {
      [&]( auto r ) {
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
    } );

    if ( !chosen_remote.has_value() ) {
      // How much current choice is better than other choices (the smaller the better)
      auto prev = chosen_remotes_.at( job ).second;

      if ( prev + static_cast<int64_t>( curr_output_size ) < 0 ) {
        return;
      }

      score = 0 - ( prev + static_cast<int64_t>( curr_output_size ) );

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

void ChildBackProp::relation_post( Handle<Relation> job, const absl::flat_hash_set<Handle<Dependee>>& dependencies )
{
  if ( base_.get().get_ep( job ) ) {
    return;
  }

  auto s = chosen_remotes_.at( job ).second;

  bool undo = job.visit<bool>( overload {
    [&]( Handle<Step> ) { return s == 0; },
    [&]( Handle<Eval> e ) {
      return e.unwrap<Object>().visit<bool>( overload {
        [&]( Handle<Value> v ) {
          return v.visit<bool>( overload {
            [&]( Handle<ValueTree> ) { return s + 2 * dependencies.size() * sizeof( Handle<Fix> ) == 0; },
            [&]( auto ) { return s == 0; },
          } );
        },
        [&]( Handle<ObjectTree> ) { return s + 2 * dependencies.size() * sizeof( Handle<Fix> ) == 0; },
        [&]( auto ) { return s == 0; },
      } );
    },
  } );

  if ( undo ) {
    if ( dependencies.size() == 1 ) {
      auto d = *dependencies.begin();
      if ( chosen_remotes_.contains( d ) ) {
        chosen_remotes_.insert_or_assign( job, { chosen_remotes_.at( d ).first, chosen_remotes_.at( d ).second } );
      } else {
        if ( !base_.get().get_contains( d ).contains( local_ ) ) {
          chosen_remotes_.insert_or_assign( job, { *base_.get().get_contains( d ).begin(), 0 } );
        }
      }
    } else {
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

      if ( remote.has_value() ) {
        chosen_remotes_.insert_or_assign( job, { remote.value(), 0 } );
      }
    }
  }
}

void ChildBackProp::relation_pre( Handle<Relation> job, const absl::flat_hash_set<Handle<Dependee>>& dependencies )
{
  for ( const auto& d : dependencies ) {
    dependees_[d].insert( Handle<Relation>( job ) );
  }
}

void InOutSource::relation_pre( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& dependencies )
{
  bool any_ep = false;
  for ( auto d : dependencies ) {
    if ( base_.get().get_ep( d ) ) {
      chosen_remotes_.at( d ) = { local_, 0 };
      any_ep = true;
    }
  }
  if ( any_ep )
    return;

  size_t assigned = 0;
  multimap<int64_t, Handle<Dependee>, greater<int64_t>> scores;

  for ( auto d : dependencies ) {
    if ( chosen_remotes_.contains( d ) and is_local( chosen_remotes_.at( d ).first ) ) {
      assigned += 1;
      scores.insert( { chosen_remotes_.at( d ).second, d } );
    }
  }

  if ( local_->get_info()->parallelism ) {
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

void RandomSelection::all( Handle<Dependee> job )
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

void SendToRemotePass::all( Handle<Dependee> job )
{
  if ( !is_local( chosen_remotes_.at( job ).first ) ) {
    VLOG( 1 ) << "Run job remotely " << job << endl;
    remote_jobs_[chosen_remotes_.at( job ).first].insert( job );
  }
}

void SendToRemotePass::relation_pre( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& dependencies )
{
  for ( auto d : dependencies ) {
    const auto& contains = base_.get().get_contains( d );
    if ( contains.empty() ) {
      if ( !is_local( chosen_remotes_.at( d ).first ) ) {
        VLOG( 1 ) << "Run job remotely " << d << endl;
        remote_jobs_[chosen_remotes_.at( d ).first].insert( d );
      }
    } else {
      if ( !contains.contains( local_ ) ) {
        remote_jobs_[*contains.begin()].insert( d );
      }
    }
  }
}

void SendToRemotePass::send_job_dependencies( shared_ptr<IRuntime> rt, Handle<Dependee> job )
{
  auto remote_contained = job.visit<bool>( overload {
    [&]( Handle<Relation> x ) { return rt->contains( x ); },
    // Leave whether to send data to not to Remote's decision
    []( auto ) { return false; },
  } );

  if ( remote_contained ) {
    return;
  }

  job.visit<void>( overload {
    [&]( Handle<Relation> r ) {
      if ( relater_.get().contains( r ) ) {
        remote_data_[rt].insert( r );
      } else {
        auto data = r.visit<optional<Handle<AnyDataType>>>(
          overload { []( Handle<Eval> e ) { return handle::data( e.unwrap<Object>() ); },
                     []( Handle<Step> ) -> optional<Handle<AnyDataType>> { return {}; } } );

        if ( data.has_value() ) {
          data.value().visit<void>(
            overload { []( Handle<Literal> ) {}, [&]( auto h ) { remote_data_[rt].insert( h ); } } );
        }

        for ( auto d : sketch_graph_.get_forward_dependencies( r ) ) {
          send_job_dependencies( rt, d );
        }
      }
    },
    [&]( Handle<AnyTreeRef> ref ) {
      if ( relater_.get().contains_shallow( relater_.get().unref( ref ) ) ) {
        ref.visit<void>( [&]( auto r ) { remote_data_[rt].insert( r ); } );
      }
    },
    [&]( auto d ) {
      if ( relater_.get().contains( d ) ) {
        remote_data_[rt].insert( d );
      }
    },
  } );
}

void SendToRemotePass::send_remote_jobs( Handle<Dependee> root )
{
  this->run( root );

  vector<pair<shared_ptr<IRuntime>, absl::flat_hash_set<Handle<Dependee>>::const_iterator>> iterators {};
  for ( const auto& [rt, handles] : remote_jobs_ ) {
    iterators.push_back( make_pair( rt, handles.begin() ) );
    for ( auto h : handles ) {
      send_job_dependencies( rt, h );
    }
  }

  for ( const auto& [rt, data] : remote_data_ ) {
    for ( auto d : data ) {
      d.visit<void>( overload {
        [&]( Handle<Relation> r ) { rt->put_force( r, relater_.get().get( r ).value() ); },
        [&]( Handle<ObjectTreeRef> r ) {
          auto tree = relater_.get().unref( r );
          rt->put_shallow( tree, relater_.get().get_shallow( tree ).value() );
        },
        [&]( Handle<ValueTreeRef> r ) {
          auto tree = relater_.get().unref( r );
          rt->put_shallow( tree, relater_.get().get_shallow( tree ).value() );
        },
        [&]( auto x ) { rt->put( x, relater_.get().get( x ).value() ); },
      } );
    }
  }

  unordered_set<shared_ptr<IRuntime>> finished;
  size_t iterator_index = 0;
  while ( finished.size() < iterators.size() ) {
    auto& [r, it] = iterators.at( iterator_index );
    if ( it != remote_jobs_.at( r ).end() ) {
      auto h = *it;
      h.visit<void>( overload {
        [&]( Handle<Relation> d ) {
          r->get( d );
          sketch_graph_.erase_forward_dependencies( d );
        },
        [&]( Handle<ObjectTreeRef> ref ) { r->get_shallow( relater_.get().unref( ref ) ); },
        [&]( Handle<ValueTreeRef> ref ) { r->get_shallow( relater_.get().unref( ref ) ); },
        [&]( auto d ) { r->get( d ); },
      } );
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

void FinalPass::data( Handle<Dependee> job )
{
  VLOG( 1 ) << "Run job " << ( is_local( chosen_remotes_.at( job ).first ) ? "locally " : "remotely " ) << job
            << endl;
  if ( is_local( chosen_remotes_.at( job ).first ) ) {
    job.visit<void>( overload {
      [&]( Handle<Relation> h ) { chosen_remotes_.at( job ).first->get( h ); },
      []( auto ) { throw std::runtime_error( "Unreachable" ); },
    } );
  }
}

void FinalPass::relation_pre( Handle<Relation> job, const absl::flat_hash_set<Handle<Dependee>>& )
{
  if ( relater_.get().contains( job ) ) {
    sketch_graph_.erase_forward_dependencies( job );
  }

  absl::flat_hash_set<Handle<Relation>> unblocked;
  sch_.get().merge_sketch_graph( job, unblocked );

  if ( unblocked.size() == 1 ) {
    auto todo = *unblocked.begin();
    if ( is_local( chosen_remotes_.at( todo ).first ) ) {
      todo_ = todo;
    }
  } else {
    for ( auto r : unblocked ) {
      if ( is_local( chosen_remotes_.at( r ).first ) ) {
        chosen_remotes_.at( r ).first->get( r );
      }
    }
  }
}

optional<Handle<Thunk>> PassRunner::run( reference_wrapper<Relater> rt,
                                         reference_wrapper<SketchGraphScheduler> sch,
                                         Handle<Dependee> top_level_job,
                                         const vector<PassType>& passes )
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
        dynamic_cast<ChildBackProp*>( selection.value().get() )->run( top_level_job );
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
  FinalPass final( base, rt, sch, move( selection.value() ) );
  final.run( top_level_job );

  if ( final.get_todo().has_value() ) {
    auto r = final.get_todo().value();
    return r.visit<optional<Handle<Thunk>>>( overload { [&]( Handle<Eval> ) -> optional<Handle<Thunk>> {
                                                         rt.get().get_local()->get( r );
                                                         return {};
                                                       },
                                                        [&]( Handle<Step> s ) -> optional<Handle<Thunk>> {
                                                          if ( top_level_job == Handle<Dependee>( r ) ) {
                                                            return s.unwrap<Thunk>();
                                                          } else {
                                                            return {};
                                                          }
                                                        } } );
  }

  return {};
}
