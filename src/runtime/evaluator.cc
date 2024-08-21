#include <functional> // IWYU pragma: keep
#include <stdexcept>

#include "evaluator.hh"
#include "overload.hh"

template<typename T>
using Result = FixRuntime::Result<T>;

using namespace std::placeholders;

Result<Value> FixEvaluator::evalStrict( Handle<Object> x )
{
  auto mapEval = [&]( auto x ) { return rt_.mapEval( x ); };
  auto evalStrict = [&]( auto x ) { return rt_.evalStrict( x ); };
  auto load = [&]( auto x ) { return rt_.load( x ); };
  auto force = [&]( auto x ) { return rt_.force( x ); };

  return x.visit<Result<Value>>( overload {
    [&]( Handle<Value> x ) { return lift( x ); },
    [&]( Handle<Thunk> x ) { return force( x ).and_then( evalStrict ); },
    [&]( Handle<ObjectTree> x ) { return mapEval( x ); },
    [&]( Handle<ObjectTreeRef> x ) {
      return load( x ).and_then( [&]( auto h ) -> Result<Value> {
        return h.template visit<Result<Value>>( overload {
          [&]( Handle<ValueTree> t ) { return lift( t ); },
          [&]( Handle<ObjectTree> t ) { return mapEval( t ); },
          []( Handle<ExpressionTree> ) -> Result<Value> {
            throw std::runtime_error( "Invalid load() return type." );
          },
        } );
      } );
    },
  } );
}

Result<Object> FixEvaluator::evalShallow( Handle<Object> x )
{
  auto evalShallow = [&]( auto x ) { return this->evalShallow( x ); };
  auto ref = [&]( auto x ) { return rt_.ref( x ); };
  auto force = [&]( auto x ) { return rt_.force( x ); };

  return x.visit<Result<Object>>( overload {
    [&]( Handle<Value> x ) { return lower( x ); },
    [&]( Handle<Thunk> x ) { return force( x ).and_then( evalShallow ); },
    [&]( Handle<ObjectTree> x ) { return ref( x ).unwrap<ObjectTreeRef>(); },
    [&]( Handle<ObjectTreeRef> x ) { return x; },
  } );
}

Result<Object> FixEvaluator::force( Handle<Thunk> x )
{
  auto mapReduce = [&]( auto x ) { return rt_.mapReduce( x ); };
  auto mapEvalShallow = [&]( auto x ) { return rt_.mapEvalShallow( x ); };
  auto apply = [&]( auto x ) { return rt_.apply( x ); };
  auto load = [&]( auto x ) { return rt_.load( x ); };
  auto loadShallow = [&]( auto x ) { return rt_.loadShallow( x ); };
  auto select = [&]( auto x ) { return rt_.select( x ); };

  return x.visit<Result<Object>>( overload {
    [&]( Handle<Identification> x ) {
      return x.unwrap<Value>()
        .visit<Result<Fix>>(
          overload { [&]( Handle<Blob> y ) { return y.visit<Result<Fix>>( [&]( auto z ) { return load( z ); } ); },
                     [&]( Handle<BlobRef> y ) {
                       return y.template unwrap<Blob>().visit<Result<Fix>>( [&]( auto z ) { return load( z ); } );
                     },
                     [&]( Handle<ValueTree> y ) {
                       return load( y ).transform( []( auto h ) { return h.template unwrap<ValueTree>(); } );
                     },
                     [&]( Handle<ValueTreeRef> y ) {
                       return load( y ).transform( []( auto h ) { return h.template unwrap<ValueTree>(); } );
                     } } )
        .and_then( []( auto h ) -> Result<Object> { return handle::extract<Object>( h ); } );
    },
    [&]( Handle<Application> x ) {
      return load( x.unwrap<ExpressionTree>() )
        .and_then( [&]( auto h ) {
          return h.template visit<Result<ObjectTree>>( overload {
            [&]( Handle<ExpressionTree> t ) { return mapReduce( t ); },
            []( Handle<ObjectTree> t ) { return t; },
            []( Handle<ValueTree> t ) { return Handle<ObjectTree>( t ); },
          } );
        } )
        .and_then( apply );
    },
    [&]( Handle<Selection> x ) {
      return loadShallow( x.unwrap<ObjectTree>() )
        .and_then( [&]( auto h ) {
          return h.template visit<Result<ObjectTree>>( overload {
            [&]( Handle<ObjectTree> t ) { return mapEvalShallow( t ); },
            []( Handle<ValueTree> t ) { return t.into<ObjectTree>(); },
            []( Handle<ExpressionTree> ) -> Result<ObjectTree> {
              throw std::runtime_error( "Invalid loadShallow return type" );
            },
          } );
        } )
        .and_then( select );
    },
  } );
}

Result<Object> FixEvaluator::reduce( Handle<Expression> x )
{
  auto evalStrict = [&]( auto x ) { return rt_.evalStrict( x ); };
  auto evalShallow = [&]( auto x ) { return this->evalShallow( x ); };
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
      if ( auto h = load( x ); h.has_value() )
        return mapLift( h.value().unwrap<ValueTree>() );
      return {};
    },
  } );
}

Result<Value> FixEvaluator::lower( Handle<Value> x )
{
  auto ref = [&]( auto x ) { return rt_.ref( x ); };

  return x.visit<Result<Value>>( overload {
    [&]( Handle<Blob> x ) {
      return x.visit<Handle<Value>>( overload {
        []( Handle<Literal> l ) { return l; },
        []( Handle<Named> n ) { return Handle<BlobRef>( Handle<Blob>( n ) ); },
      } );
    },
    [&]( Handle<ValueTree> x ) { return ref( x ).unwrap<ValueTreeRef>(); },
    [&]( Handle<BlobRef> x ) { return x; },
    [&]( Handle<ValueTreeRef> x ) { return x; },
  } );
}

Result<Object> FixEvaluator::relate( Handle<Fix> x )
{
  auto evalStrict = [&]( auto x ) { return rt_.evalStrict( x ); };
  auto force = [&]( auto x ) { return rt_.force( x ); };
  auto reduce = [&]( auto x ) { return this->reduce( x ); };

  return x.visit<Result<Object>>( overload {
    [&]( Handle<Relation> x ) {
      return x.visit<Result<Object>>( overload {
        [&]( Handle<Think> x ) { return force( x.unwrap<Thunk>() ); },
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
