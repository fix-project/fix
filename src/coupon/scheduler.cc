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

ApplyTag LocalScheduler::force( Handle<Thunk> thunk )
{
  return thunk.visit<ApplyTag>(
    overload { [&]( Handle<Identification> ) -> ApplyTag { throw std::runtime_error( "Unimplemetned" ); },
               [&]( Handle<Selection> ) -> ApplyTag { throw std::runtime_error( "Unimplemetned" ); },
               [&]( Handle<Application> x ) {
                 auto t = load( rt_.unwrap( x ) );
                 auto equivtag = mapReduce( t );
                 auto applytag = apply( equivtag.rhs.unwrap<Value>().unwrap<Treeish>().unwrap<Tree>() );
                 return collector_.equivalence_apply( equivtag, applytag ).value();
               } } );
}

ApplyTag LocalScheduler::force_until_not_thunk( Handle<Thunk> thunk )
{
  auto applytag = force( thunk );

  while ( true ) {
    if ( auto x = applytag.result.try_into<Thunk>(); x.has_value() ) {
      auto equiv = collector_.apply_to_thunk_equivalence( applytag );
      applytag = collector_.equivalence_apply( equiv.value(), force( x.value() ) ).value();
    } else {
      break;
    }
  }

  return applytag;
}

EquivalenceTag LocalScheduler::evalShallow( Handle<Thunk> thunk )
{
  auto applytag = force_until_not_thunk( thunk );
  return collector_.apply_to_shallow_encode_equivalence( applytag ).value();
}

EvalTag LocalScheduler::evalStrict( Handle<Fix> expression )
{
  return expression.visit<EvalTag>( overload {
    [&]( Handle<Value> x ) {
      return x.visit<EvalTag>( overload {
        [&]( Handle<Blob> x ) { return collector_.get_eval_identity( x ); },
        [&]( Handle<Treeish> x ) { return mapEval( x ); },
        []( Handle<BlobRef> ) -> EvalTag { throw runtime_error( "Unimplemetned" ); },
        []( Handle<TreeishRef> ) -> EvalTag { throw runtime_error( "Unimplemented" ); },
      } );
    },
    [&]( Handle<Thunk> x ) {
      auto applytag = force( x );
      auto evaltag = evalStrict( applytag.result );
      return collector_.apply_to_strict_encode_equivalence( applytag, evaltag ).value();
    },
    [&]( Handle<Encode> ) -> EvalTag { throw runtime_error( "Unreachable" ); },
  } );
}

EquivalenceTag LocalScheduler::reduce( Handle<Fix> x )
{
  return x.visit<EquivalenceTag>( overload {
    [&]( Handle<Encode> x ) {
      return x.visit<EquivalenceTag>( overload {
        [&]( Handle<Strict> x ) {
          return collector_.eval_to_strict_encode_equivalence( this->evalStrict( rt_.unwrap( x ) ) ).value();
        },
        [&]( Handle<Shallow> x ) { return this->evalShallow( rt_.unwrap( x ) ); },
      } );
    },
    [&]( auto x ) { return collector_.get_equivalence_reflex( x ); } } );
}

EquivalenceTag LocalScheduler::mapReduce( Handle<Treeish> tree )
{
  auto data = rt_.attach( tree );

  vector<EquivalenceTag> to_trade;

  for ( auto x : data->span() ) {
    to_trade.push_back( reduce( x ) );
  }

  auto new_equiv = collector_.tree_equivalence( to_trade );

  tree.visit<void>( overload {
    []( Handle<Tree> ) {},
    [&]( Handle<Tag> x ) { new_equiv = tagger_.exchange_tag( x, new_equiv ).value(); },
  } );

  return new_equiv;
}

EvalTag LocalScheduler::mapEval( Handle<Treeish> tree )
{
  auto data = rt_.attach( tree );

  vector<EvalTag> to_trade;

  for ( auto x : data->span() ) {
    to_trade.push_back( this->evalStrict( x ) );
  }

  auto new_eval = collector_.tree_eval( to_trade );

  tree.visit<void>( overload { []( Handle<Tree> ) {},
                               [&]( Handle<Tag> x ) { new_eval = tagger_.exchange_tag( x, new_eval ).value(); } } );

  return new_eval;
}

EvalTag LocalScheduler::schedule( Handle<Fix> top_level_job )
{
  return evalStrict( top_level_job );
}
