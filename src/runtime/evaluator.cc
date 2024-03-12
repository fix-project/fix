#include <functional> // IWYU pragma: keep

#include "evaluator.hh"
#include "overload.hh"

template<typename T>
using Result = FixRuntime::Result<T>;

using namespace std::placeholders;

Result<Value> FixEvaluator::evalStrict( Handle<Object> x )
{
  auto mapEval = [&]( auto x ) { return rt_.mapEval( x ); };
  auto evalStrict = [&]( auto x ) { return rt_.evalStrict( x ); };

  return x.visit<Result<Value>>( overload {
    [&]( Handle<Value> x ) { return lift( x ); },
    [&]( Handle<Thunk> x ) { return force( x ).and_then( evalStrict ); },
    [&]( Handle<ObjectTree> x ) { return mapEval( x ); },
    [&]( Handle<ObjectTreeRef> x ) { return mapEval( x.unwrap<ObjectTree>() ); },
  } );
}

Result<Object> FixEvaluator::evalShallow( Handle<Object> x )
{
  auto evalShallow = [&]( auto x ) { return rt_.evalShallow( x ); };

  return x.visit<Result<Object>>( overload {
    [&]( Handle<Value> x ) { return lower( x ); },
    [&]( Handle<Thunk> x ) { return force( x ).and_then( evalShallow ); },
    [&]( Handle<ObjectTree> x ) { return Handle<ObjectTreeRef>( x ); },
    [&]( Handle<ObjectTreeRef> x ) { return x; },
  } );
}

Result<Object> FixEvaluator::force( Handle<Thunk> x )
{
  auto mapReduce = [&]( auto x ) { return rt_.mapReduce( x ); };
  auto apply = [&]( auto x ) { return rt_.apply( x ); };
  auto load = [&]( auto x ) { return rt_.load( x ); };

  return x.visit<Result<Object>>( overload {
    [&]( Handle<Identification> x ) {
      return x.unwrap<Value>()
        .visit<Result<Fix>>(
          overload { [&]( Handle<Blob> y ) { return y.visit<Result<Fix>>( [&]( auto z ) { return load( z ); } ); },
                     [&]( Handle<ValueTree> y ) { return load( y ); },
                     [&]( Handle<BlobRef> y ) { return y; },
                     [&]( Handle<ValueTreeRef> y ) { return y; } } )
        .and_then( []( auto h ) -> Result<Object> { return handle::extract<Object>( h ); } );
    },
    [&]( Handle<Application> x ) {
      return load( x.unwrap<ExpressionTree>() )
        .and_then( []( auto h ) { return handle::extract<ExpressionTree>( h ); } )
        .and_then( mapReduce )
        .and_then( apply );
    },
    [&]( Handle<Selection> ) -> Handle<Object> { throw std::runtime_error( "unimplemented" ); },
  } );
}

Result<Object> FixEvaluator::reduce( Handle<Expression> x )
{
  auto evalStrict = [&]( auto x ) { return rt_.evalStrict( x ); };
  auto evalShallow = [&]( auto x ) { return rt_.evalShallow( x ); };
  auto mapReduce = [&]( auto x ) { return rt_.mapReduce( x ); };

  return x.visit<Result<Object>>( overload {
    [&]( Handle<Object> x ) { return x; },
    [&]( Handle<Encode> x ) {
      return x.visit<Result<Object>>( overload {
        [&]( Handle<Strict> x ) { return evalStrict( x.unwrap<Thunk>() ); },
        [&]( Handle<Shallow> x ) { return evalShallow( x.unwrap<Thunk>() ); },
      } );
    },
    [&]( Handle<ExpressionTree> x ) { return mapReduce( x ); },
  } );
}

Result<Value> FixEvaluator::lift( Handle<Value> x )
{
  auto mapLift = [&]( auto x ) { return rt_.mapLift( x ); };
  auto load = [&]( auto x ) { return rt_.load( x ); };

  return x.visit<Result<Value>>( overload {
    [&]( Handle<Blob> x ) { return x; },
    [&]( Handle<ValueTree> x ) -> Result<ValueTree> { return mapLift( x ); },
    [&]( Handle<BlobRef> x ) -> Result<Blob> {
      auto blob = x.unwrap<Blob>();
      if ( blob.visit<Result<Fix>>( [&]( auto h ) { return load( h ); } ) )
        return blob;
      return {};
    },
    [&]( Handle<ValueTreeRef> x ) -> Result<ValueTree> {
      auto tree = x.unwrap<ValueTree>();
      if ( load( tree ) )
        return mapLift( tree );
      return {};
    },
  } );
}

Result<Value> FixEvaluator::lower( Handle<Value> x )
{
  return x.visit<Result<Value>>( overload {
    [&]( Handle<Blob> x ) { return Handle<BlobRef>( x ); },
    [&]( Handle<ValueTree> x ) { return Handle<ValueTreeRef>( x ); },
    [&]( Handle<BlobRef> x ) { return x; },
    [&]( Handle<ValueTreeRef> x ) { return x; },
  } );
}

Result<Object> FixEvaluator::relate( Handle<Fix> x )
{
  auto evalStrict = [&]( auto x ) { return rt_.evalStrict( x ); };
  auto mapReduce = [&]( auto x ) { return rt_.mapReduce( x ); };
  auto apply = [&]( auto x ) { return rt_.apply( x ); };
  auto reduce = [&]( auto x ) { return this->reduce( x ); };

  return x.visit<Result<Object>>( overload {
    [&]( Handle<Relation> x ) {
      return x.visit<Result<Object>>( overload {
        [&]( Handle<Apply> x ) { return mapReduce( x.unwrap<ExpressionTree>() ).and_then( apply ); },
        [&]( Handle<Eval> x ) { return evalStrict( x.unwrap<Object>() ); },
      } );
    },
    [&]( Handle<Expression> x ) { return reduce( x ); },
  } );
}

Result<Value> FixEvaluator::eval( Handle<Fix> x )
{
  auto evalStrict = [&]( auto x ) { return rt_.evalStrict( x ); };
  auto lift = [&]( auto x ) { return this->lift( x ); };

  return relate( x ).and_then( evalStrict ).and_then( lift );
}
