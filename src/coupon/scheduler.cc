#include "scheduler.hh"
#include "couponcollector.hh"
#include "overload.hh"

#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;

Handle<Blob> LocalScheduler::load( Handle<Blob> blob )
{
  rt_.load( blob );
  return blob;
}

Handle<Treeish> LocalScheduler::load( Handle<Treeish> tree )
{
  rt_.load( tree );
  return tree;
}

Handle<Treeish> LocalScheduler::load( Handle<TreeishRef> tref )
{
  return rt_.load( tref );
}

Handle<TreeishRef> LocalScheduler::ref( Handle<Treeish> tree )
{
  return rt_.ref( tree );
}

ApplyTag LocalScheduler::apply( Handle<Tree> combination )
{
  auto tree = rt_.attach( combination );
  auto res = rt_.execute( tree->at( 1 ).unwrap<Value>().unwrap<Blob>(), combination );

  auto applytag = collector_.execution_to_apply( res );
  return applytag.value();
}

ThinkTag LocalScheduler::force( Handle<Thunk> thunk )
{
  return thunk.visit<ThinkTag>( overload {
    [&]( Handle<Identification> x ) {
      auto t = load( rt_.unwrap( x ) );
      auto reducetag = reduce( t );
      auto identifytag = collector_.identify( reducetag.rhs.unwrap<Value>() );
      return collector_.identify_to_think( reducetag, identifytag ).value();
    },
    [&]( Handle<Selection> x ) {
      auto t = load( rt_.unwrap( x ) );
      auto reducetag = reduce( t );
      auto selecttag = collector_.select( reducetag.rhs.unwrap<Value>().unwrap<Treeish>().unwrap<Tree>() );
      return collector_.select_to_think( reducetag, selecttag ).value();
    },
    [&]( Handle<Application> x ) {
      auto t = load( rt_.unwrap( x ) );
      auto reducetag = map_reduce( t );
      auto applytag = apply( reducetag.rhs.unwrap<Value>().unwrap<Treeish>().unwrap<Tree>() );
      return collector_.apply_to_think( reducetag, applytag ).value();
    },
  } );
}

ThinkTag LocalScheduler::force_until_not_thunk( Handle<Thunk> thunk )
{
  auto thinktag = force( thunk );

  while ( true ) {
    if ( auto x = thinktag.result.try_into<Thunk>(); x.has_value() ) {
      auto newthinktag = force( x.value() );
      thinktag = collector_.think_transitive( thinktag, newthinktag ).value();
    } else {
      break;
    }
  }

  return thinktag;
}

ReduceTag LocalScheduler::reduce_shallow( Handle<Thunk> thunk )
{
  auto thinktag = force_until_not_thunk( thunk );
  return collector_.think_to_shallow_encode_reduce( thinktag ).value();
}

ReduceTag LocalScheduler::reduce_strict( Handle<Thunk> thunk )
{
  auto thinktag = force_until_not_thunk( thunk );
  auto evaltag = eval( thinktag.result );
  return collector_.think_to_strict_encode_reduce( thinktag, evaltag ).value();
}

EvalTag LocalScheduler::eval( Handle<Fix> expression )
{
  return expression.visit<EvalTag>( overload {
    [&]( Handle<Value> x ) {
      return x.visit<EvalTag>( overload {
        [&]( Handle<Blob> x ) { return collector_.get_eval_identity( x ); },
        [&]( Handle<Treeish> x ) { return map_eval( x ); },
        []( Handle<BlobRef> ) -> EvalTag { throw runtime_error( "Unimplemetned" ); },
        []( Handle<TreeishRef> ) -> EvalTag { throw runtime_error( "Unimplemented" ); },
      } );
    },
    [&]( Handle<Thunk> x ) {
      auto thinktag = force_until_not_thunk( x );
      auto evaltag = eval( thinktag.result );
      return collector_.think_to_eval_thunk( thinktag, evaltag ).value();
    },
    [&]( Handle<Encode> ) -> EvalTag { throw runtime_error( "Unreachable" ); },
  } );
}

ReduceTag LocalScheduler::reduce( Handle<Fix> x )
{
  return x.visit<ReduceTag>(
    overload { [&]( Handle<Encode> x ) {
                return x.visit<ReduceTag>( overload {
                  [&]( Handle<Strict> x ) { return reduce_strict( rt_.unwrap( x ) ); },
                  [&]( Handle<Shallow> x ) { return reduce_shallow( rt_.unwrap( x ) ); },
                } );
              },
               [&]( Handle<Value> x ) {
                 return x.visit<ReduceTag>( overload {
                   [&]( Handle<Blob> x ) { return collector_.get_reduce_identity( x ); },
                   [&]( Handle<Treeish> x ) { return map_reduce( x ); },
                   []( Handle<BlobRef> ) -> ReduceTag { throw runtime_error( "Unimplemetned" ); },
                   []( Handle<TreeishRef> ) -> ReduceTag { throw runtime_error( "Unimplemented" ); },
                 } );
               },
               [&]( auto x ) { return collector_.get_reduce_identity( x ); } } );
}

ReduceTag LocalScheduler::map_reduce( Handle<Treeish> tree )
{
  auto data = rt_.attach( tree );

  vector<ReduceTag> to_trade;

  for ( auto x : data->span() ) {
    to_trade.push_back( reduce( x ) );
  }

  auto new_reduce = collector_.tree_reduce( to_trade );

  tree.visit<void>( overload {
    []( Handle<Tree> ) {},
    [&]( Handle<Tag> x ) { new_reduce = tagger_.exchange_tag( x, new_reduce ).value(); },
  } );

  return new_reduce;
}

EvalTag LocalScheduler::map_eval( Handle<Treeish> tree )
{
  auto data = rt_.attach( tree );

  vector<EvalTag> to_trade;

  for ( auto x : data->span() ) {
    to_trade.push_back( this->eval( x ) );
  }

  auto new_eval = collector_.tree_eval( to_trade );

  tree.visit<void>( overload { []( Handle<Tree> ) {},
                               [&]( Handle<Tag> x ) { new_eval = tagger_.exchange_tag( x, new_eval ).value(); } } );

  return new_eval;
}

EvalTag LocalScheduler::schedule( Handle<Fix> top_level_job )
{
  return eval( top_level_job );
}
