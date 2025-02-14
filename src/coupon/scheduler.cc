#include "scheduler.hh"
#include "couponcollector.hh"
#include "overload.hh"

#include <optional>
#include <stdexcept>
#include <vector>

using namespace std;

optional<Handle<Blob>> LocalScheduler::load( Handle<Blob> blob )
{
  return rt_.load( blob );
}

optional<Handle<Blob>> LocalScheduler::load( Handle<BlobRef> blob )
{
  return load( rt_.unref( blob ) );
}

optional<Handle<Treeish>> LocalScheduler::load( Handle<Treeish> tree )
{
  return rt_.load( tree );
}

optional<Handle<Treeish>> LocalScheduler::load( Handle<TreeishRef> tref )
{
  return load( rt_.unref( tref ) );
}

Handle<TreeishRef> LocalScheduler::ref( Handle<Treeish> tree )
{
  return rt_.ref( tree );
}

optional<ApplyTag> LocalScheduler::apply( Handle<Tree> combination )
{
  if ( !apply_results_.empty()
       && equal( rt_.get_name( apply_results_.back().combination ), rt_.get_name( combination ) ) ) {
    auto applytag = collector_.execution_to_apply( apply_results_.back() );
    apply_results_.pop_back();
    return applytag;
  }

  to_apply_.push_back( combination );
  return {};
}

optional<ThinkTag> LocalScheduler::force_until_not_thunk( Handle<Thunk> thunk )
{
  optional<Handle<Thunk>> to_think = thunk;
  optional<ThinkTag> state;

  if ( !continuation_.empty() ) {
    if ( continuation_.back().has_value() ) {
      state = get<ThinkTag>( continuation_.back().value() );
      to_think = state->result.try_into<Thunk>();
    }
    continuation_.pop_back();
  }

  while ( to_think.has_value() ) {
    auto newthinktag = force( to_think.value() );
    if ( newthinktag.has_value() ) {
      if ( state.has_value() ) {
        state = collector_.think_transitive( state.value(), newthinktag.value() ).value();
      } else {
        state = newthinktag.value();
      }
    } else {
      new_continuation_.push_back( state );
      return {};
    }

    to_think = state->result.try_into<Thunk>();
  }

  return state;
}

optional<ThinkTag> LocalScheduler::force( Handle<Thunk> thunk )
{
  // Step 1: load tree combination
  // Step 2: reduce the tree => ReduceTag
  // Step 3: Work => Done
  auto state = optional<CouponCollectorTag>();
  if ( !continuation_.empty() ) {
    state = continuation_.back();
  }

  if ( !continuation_.empty() ) {
    continuation_.pop_back();
  }

  auto t = thunk.visit<optional<Handle<Treeish>>>( [&]( auto x ) { return load( rt_.unwrap( x ) ); } );

  if ( !t.has_value() ) {
    new_continuation_.push_back( {} );
    return {};
  }

  // State is either {} or a ReduceTag?
  auto reducetag = state.has_value() ? get<ReduceTag>( state.value() ) : reduce( t.value() );
  if ( !reducetag.has_value() ) {
    new_continuation_.push_back( {} );
    return {};
  }

  auto result = thunk.visit<optional<ThinkTag>>( overload {
    [&]( Handle<Identification> ) {
      auto identifytag = collector_.identify( reducetag->rhs.unwrap<Value>() );
      return collector_.identify_to_think( reducetag.value(), identifytag ).value();
    },
    [&]( Handle<Selection> ) {
      auto selecttag = collector_.select( reducetag->rhs.unwrap<Value>().unwrap<Treeish>().unwrap<Tree>() );
      return collector_.select_to_think( reducetag.value(), selecttag ).value();
    },
    [&]( Handle<Application> ) {
      auto applytag = apply( reducetag->rhs.unwrap<Value>().unwrap<Treeish>().unwrap<Tree>() );
      if ( applytag.has_value() ) {
        return collector_.apply_to_think( reducetag.value(), applytag.value() );
      } else {
        return optional<ThinkTag>();
      }
    },
  } );

  if ( !result.has_value() ) {
    new_continuation_.push_back( reducetag.value() );
  }

  return result;
}

optional<ReduceTag> LocalScheduler::reduce_shallow( Handle<Thunk> thunk )
{
  auto thinktag = force_until_not_thunk( thunk );

  if ( !thinktag.has_value() ) {
    return {};
  }

  return collector_.think_to_shallow_encode_reduce( thinktag.value() ).value();
}

pair<optional<ThinkTag>, optional<EvalTag>> LocalScheduler::thunk_strict( Handle<Thunk> thunk )
{
  // Step 1: think transitively until not thunk
  // Step 2: eval the rhs of thinktag
  auto state = continuation_.empty() ? optional<CouponCollectorTag>() : continuation_.back();

  if ( !continuation_.empty() ) {
    continuation_.pop_back();
  }

  auto thinktag = state.has_value() ? get<ThinkTag>( state.value() ) : force_until_not_thunk( thunk );

  if ( !thinktag.has_value() ) {
    new_continuation_.push_back( {} );
    return { {}, {} };
  }

  auto evaltag = eval( thinktag->result );

  if ( !evaltag.has_value() ) {
    new_continuation_.push_back( thinktag.value() );
    return { thinktag, {} };
  }

  return { thinktag, evaltag };
}

optional<ReduceTag> LocalScheduler::reduce_strict( Handle<Thunk> thunk )
{
  auto tags = thunk_strict( thunk );
  if ( tags.first.has_value() and tags.second.has_value() ) {
    return collector_.think_to_strict_encode_reduce( tags.first.value(), tags.second.value() ).value();
  }

  return {};
}

optional<EvalTag> LocalScheduler::eval( Handle<Fix> expression )
{
  return expression.visit<optional<EvalTag>>( overload {
    [&]( Handle<Value> x ) -> optional<EvalTag> {
      return x.visit<optional<EvalTag>>( overload {
        [&]( Handle<Blob> x ) { return collector_.get_eval_identity( x ); },
        [&]( Handle<Treeish> x ) { return map_eval( x ); },
        []( Handle<BlobRef> ) -> EvalTag { throw runtime_error( "Unimplemetned" ); },
        []( Handle<TreeishRef> ) -> EvalTag { throw runtime_error( "Unimplemented" ); },
      } );
    },
    [&]( Handle<Thunk> x ) -> optional<EvalTag> {
      auto tags = thunk_strict( x );
      if ( tags.first.has_value() and tags.second.has_value() ) {
        return collector_.think_to_eval_thunk( tags.first.value(), tags.second.value() ).value();
      }
      return optional<EvalTag>();
    },
    [&]( Handle<Encode> ) -> EvalTag { throw runtime_error( "Unreachable" ); },
  } );
}

optional<ReduceTag> LocalScheduler::reduce( Handle<Fix> x )
{
  return x.visit<optional<ReduceTag>>(
    overload { [&]( Handle<Encode> x ) {
                return x.visit<optional<ReduceTag>>( overload {
                  [&]( Handle<Strict> x ) { return reduce_strict( rt_.unwrap( x ) ); },
                  [&]( Handle<Shallow> x ) { return reduce_shallow( rt_.unwrap( x ) ); },
                } );
              },
               [&]( Handle<Value> x ) {
                 return x.visit<optional<ReduceTag>>( overload {
                   [&]( Handle<Blob> x ) { return collector_.get_reduce_identity( x ); },
                   [&]( Handle<Treeish> x ) { return map_reduce( x ); },
                   []( Handle<BlobRef> ) -> ReduceTag { throw runtime_error( "Unimplemetned" ); },
                   []( Handle<TreeishRef> ) -> ReduceTag { throw runtime_error( "Unimplemented" ); },
                 } );
               },
               [&]( auto x ) { return collector_.get_reduce_identity( x ); } } );
}

optional<ReduceTag> LocalScheduler::map_reduce( Handle<Treeish> tree )
{
  auto data = rt_.attach( tree );

  vector<optional<ReduceTag>> reduce_tags { data->span().size() };

  if ( !continuation_.empty() ) {
    for ( size_t i = 0; i < data->span().size(); i++ ) {
      reduce_tags[i]
        = continuation_.back().has_value() ? get<ReduceTag>( continuation_.back().value() ) : optional<ReduceTag>();
      continuation_.pop_back();
    }
  }

  size_t i = 0;
  bool all_ready = true;

  auto tmp_new_continuation = new_continuation_;
  deque<deque<std::optional<CouponCollectorTag>>> new_continuations;
  for ( auto x : data->span() ) {
    if ( !reduce_tags[i].has_value() ) {
      new_continuation_ = {};
      reduce_tags[i] = reduce( x );
      all_ready &= reduce_tags[i].has_value();
      if ( !reduce_tags[i].has_value() ) {
        new_continuations.push_back( new_continuation_ );
      }
    }
    i++;
  }
  new_continuation_ = tmp_new_continuation;

  if ( all_ready ) {
    vector<ReduceTag> to_trade;
    for ( const auto& x : reduce_tags ) {
      to_trade.push_back( x.value() );
    }

    auto new_reduce = collector_.tree_reduce( to_trade );

    tree.visit<void>( overload {
      []( Handle<Tree> ) {},
      [&]( Handle<Tag> x ) { new_reduce = tagger_.exchange_tag( x, new_reduce ).value(); },
    } );

    return new_reduce;
  } else {
    for ( int i = new_continuations.size() - 1; i >= 0; i-- ) {
      auto nc = new_continuations[i];
      for ( auto x : nc ) {
        new_continuation_.push_back( x );
      }
    }

    for ( int i = data->span().size() - 1; i >= 0; i-- ) {
      new_continuation_.push_back( reduce_tags[i] );
    }
    return {};
  }
}

optional<EvalTag> LocalScheduler::map_eval( Handle<Treeish> tree )
{
  auto data = rt_.attach( tree );

  vector<optional<EvalTag>> eval_tags { data->span().size() };

  if ( !continuation_.empty() ) {
    for ( size_t i = 0; i < data->span().size(); i++ ) {
      eval_tags[i]
        = continuation_.back().has_value() ? get<EvalTag>( continuation_.back().value() ) : optional<EvalTag>();
      continuation_.pop_back();
    }
  }

  size_t i = 0;
  bool all_ready = true;

  auto tmp_new_continuation = new_continuation_;
  deque<deque<std::optional<CouponCollectorTag>>> new_continuations;
  for ( auto x : data->span() ) {
    if ( !eval_tags[i].has_value() ) {
      new_continuation_ = {};
      eval_tags[i] = eval( x );
      all_ready &= eval_tags[i].has_value();
      if ( !eval_tags[i].has_value() ) {
        new_continuations.push_back( new_continuation_ );
      }
    }
    i++;
  }
  new_continuation_ = tmp_new_continuation;

  if ( all_ready ) {
    vector<EvalTag> to_trade;
    for ( const auto& x : eval_tags ) {
      to_trade.push_back( x.value() );
    }

    auto new_eval = collector_.tree_eval( to_trade );

    tree.visit<void>( overload {
      []( Handle<Tree> ) {}, [&]( Handle<Tag> x ) { new_eval = tagger_.exchange_tag( x, new_eval ).value(); } } );

    return new_eval;
  } else {
    for ( int i = new_continuations.size() - 1; i >= 0; i-- ) {
      auto nc = new_continuations[i];
      for ( auto x : nc ) {
        new_continuation_.push_back( x );
      }
    }

    for ( int i = data->span().size() - 1; i >= 0; i-- ) {
      new_continuation_.push_back( eval_tags[i] );
    }
    return {};
  }
}

EvalTag LocalScheduler::schedule( Handle<Fix> top_level_job,
                                  deque<optional<CouponCollectorTag>> continuation,
                                  deque<KernelExecutionTag> results )
{
  continuation_ = continuation;

  to_apply_ = {};
  new_continuation_ = {};

  apply_results_ = results;

  auto result = eval( top_level_job );

  if ( result.has_value() ) {
    return result.value();
  } else {
    results = {};
    for ( auto& apply : to_apply_ ) {
      results.push_back( rt_.execute( rt_.attach( apply )->span()[1].unwrap<Value>().unwrap<Blob>(), apply ) );
    }

    return this->schedule( top_level_job, new_continuation_, results );
  }
}
