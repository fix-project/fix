#pragma once
#include <optional>

#include "handle.hh"

class FixRuntime
{
public:
  template<typename T>
  using Result = std::optional<Handle<T>>;

  virtual Result<Blob> load( Handle<Blob> value ) = 0;
  // load( Handle<ValueTree> ) -> Handle<ValueTree>
  // load( Handle<ObjectTree> ) -> Handle<ObjectTree> or Handle<ValueTree>
  // load( Handle<ExpressionTree> -> Handle<ExpressionTree> or Handle<ObjectTree> or Handle<ValueTree>
  virtual Result<AnyTree> load( Handle<AnyTree> value ) = 0;
  virtual Result<AnyTree> load( Handle<AnyTreeRef> treeref ) = 0;
  virtual Handle<AnyTreeRef> ref( Handle<AnyTree> tree ) = 0;
  virtual Result<Object> apply( Handle<ObjectTree> combination ) = 0;
  virtual Result<Value> evalStrict( Handle<Object> expression ) = 0;
  virtual Result<Object> evalShallow( Handle<Object> expression ) = 0;
  virtual Result<ValueTree> mapEval( Handle<ObjectTree> tree ) = 0;
  virtual Result<ObjectTree> mapReduce( Handle<ExpressionTree> tree ) = 0;
  virtual Result<ValueTree> mapLift( Handle<ValueTree> tree ) = 0;

  virtual ~FixRuntime() {};
};

class FixEvaluator
{
  FixRuntime& rt_;

public:
  FixEvaluator( FixRuntime& rt )
    : rt_( rt )
  {}

  template<typename T>
  using Result = FixRuntime::Result<T>;

  Result<Value> evalStrict( Handle<Object> x );
  Result<Object> evalShallow( Handle<Object> x );
  Result<Object> force( Handle<Thunk> x );
  Result<Object> reduce( Handle<Expression> x );
  Result<Value> lift( Handle<Value> x );
  Result<Value> lower( Handle<Value> x );

  Result<Object> relate( Handle<Fix> x );
  Result<Value> eval( Handle<Fix> x );
};
