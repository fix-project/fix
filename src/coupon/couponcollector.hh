#pragma once

#include "handle.hh"
#include <optional>
#include <vector>

struct EquivalenceTag
{
  // Invariant: lhs and rhs is of the same visibility level
  Handle<Fix> lhs;
  Handle<Fix> rhs;
};

struct EvalTag
{
  // Invariant: rhs does not have any reachable Handle<Thunk> or Handle<Encode>
  Handle<Fix> lhs;
  Handle<Value> rhs;
};

struct ApplyTag
{
  Handle<Tree> combination;
  Handle<Fix> result;
};

struct IdentifyTag
{
  Handle<Value> combination;
  Handle<Value> result;
};

struct SelectTag
{
  Handle<Value> combination;
  Handle<Fix> result;
};

struct ThinkTag
{
  Handle<Thunk> thunk;
  Handle<Fix> result;
};

struct ReduceTag
{
  // Invariant: rhs does not have any reachable Handle<Thunk>
  Handle<Fix> lhs;
  Handle<Fix> rhs;
};

using CouponCollectorTag
  = std::variant<EquivalenceTag, EvalTag, ReduceTag, ApplyTag, IdentifyTag, SelectTag, ThinkTag>;

class DeterministicRuntime;
struct KernelExecutionTag;

class CouponCollector
{
  DeterministicRuntime& rt_;

private:
  Handle<Tag> to_fix_tag( const CouponCollectorTag& tag );
  CouponCollectorTag to_coupon_collector_tag( Handle<Tag> );

  const Handle<Fix> equivalence_ = Handle<Literal>( "Equivalence" );
  const Handle<Fix> eval_ = Handle<Literal>( "Eval" );
  const Handle<Fix> think_ = Handle<Literal>( "Think" );
  const Handle<Fix> reduce_ = Handle<Literal>( "Reduce" );
  const Handle<Fix> identify_ = Handle<Literal>( "Identify" );
  const Handle<Fix> select_ = Handle<Literal>( "Select" );
  const Handle<Fix> apply_ = Handle<Literal>( "Apply" );

public:
  // {} => Eval( x, x )
  EvalTag get_eval_identity( Handle<Blob> x );

  // {} => Reduce( Blob x, Blob x )
  ReduceTag get_reduce_identity( Handle<Blob> x );
  // {} => Reduce( Thunk x, Thunk x )
  ReduceTag get_reduce_identity( Handle<Thunk> x );

  // {} => Equivalence( x, x )
  EquivalenceTag get_equivalence_reflexive( Handle<Fix> x );
  // {Equivalence( x, y )} => Equivalence( y, x )
  EquivalenceTag get_equivalence_symmetric( EquivalenceTag );
  // {Equivalence( x, y ), Equivalence( y, z )}  => Equivalence( x, z )
  std::optional<EquivalenceTag> get_equivalence_transitive( EquivalenceTag, EquivalenceTag );

  // {Equivalence( x, y ), Eval( y, z )}  => Eval( x, z )
  std::optional<EvalTag> equivalence_eval( EquivalenceTag, EvalTag );
  // {Equivalence( x, y ), Reduce( y, z )} => Reduce( x, z )
  std::optional<ReduceTag> equivalence_reduce( EquivalenceTag, ReduceTag );
  // {Equivalence( x, y ), Think( y, z )} => Think( x, z )
  std::optional<ThinkTag> equivalence_think( EquivalenceTag, ThinkTag );

  // Note: this exchange works for EvalTag and ReduceTag iff for any EquivalendeTag, its lhs is Thunk/Encode iff its
  // rhs is Thunk/Encode, and this condition currently holds given the rest of the exchange rules. This exchange
  // does not hold for ThinkTag, since ThinkTag implies the forward progressing direction of evaluations, and
  // ThinkTag{ x, x } => _|_.
  //
  // {Equivalence( x, z ), Eval( y, z )} => Eval( y, x )
  std::optional<EvalTag> equivalence_eval_result( EquivalenceTag, EvalTag );
  // {Equivalence( x, z ), Reduce( y, z )} => Reduce( y, x )
  std::optional<ReduceTag> equivalence_reduce_result( EquivalenceTag, ReduceTag );

  // {[Equivalence(v_i1, v_i2)]} => Equivalence([v_i1], [v_i2])
  EquivalenceTag tree_equivalence( std::vector<EquivalenceTag> tags );
  // {[Eval(v_i1, v_i2)]} => Eval([v_i1], [v_i2])
  EvalTag tree_eval( std::vector<EvalTag> tags );
  // {[Reduce(v_i1, v_i2)]} => Reduce([v_i1], [v_i2])
  ReduceTag tree_reduce( std::vector<ReduceTag> tags );

  // KernelExecutionTag => ApplyTag
  std::optional<ApplyTag> execution_to_apply( KernelExecutionTag );
  SelectTag select( Handle<Tree> combination );
  IdentifyTag identify( Handle<Value> value );

  // { Reduce( Tree x, Tree y ), Apply( Tree y, z )} => Think( ApplicationThunk x, z )
  std::optional<ThinkTag> apply_to_think( ReduceTag, ApplyTag );
  // { Reduce( Tree x, Tree y ), Select( Tree y, z )} => Think( SelectionThunk x, z )
  std::optional<ThinkTag> select_to_think( ReduceTag, SelectTag );
  // { Reduce( x, y ), Identify( y, z )} => Think( IdentificationThunk x, z )
  std::optional<ThinkTag> identify_to_think( ReduceTag, IdentifyTag );

  // { Think( x, y ), Think( y, z ) } => Think( x, z )
  std::optional<ThinkTag> think_transitive( ThinkTag, ThinkTag );

  // {Think( Thunk x, Thunk y )} => Equivalence( Thunk x, Thunk y )
  std::optional<EquivalenceTag> think_to_thunk_equivalence( ThinkTag );
  // {Think( Thunk x, Value y )} => Reduce( Shallow(Thunk x), ref( y ) )
  std::optional<ReduceTag> think_to_shallow_encode_reduce( ThinkTag );
  // {Think( Thunk x, y ), Eval(y, z)} => Reduce( Strict(Thunk x), z )
  std::optional<ReduceTag> think_to_strict_encode_reduce( ThinkTag, EvalTag );
  // {Think( Thunk x, y ), Eval(y, z)} => Eval(Thunk x, z )
  std::optional<EvalTag> think_to_eval_thunk( ThinkTag, EvalTag );
  // {Eval( Thunk x, y )} => Reduce(Strict(Thunk x), z)
  std::optional<ReduceTag> eval_thunk_to_strict_encode_reduce( EvalTag );

  // {Equivalence(Tree [v_1], Tree [v_2])} => Equivalence(v_1i, v_2i)
  std::optional<EquivalenceTag> tree_equiv_to_entry_equiv( EquivalenceTag, size_t i );
  // {Reduce (Tree [v_1], Tree [v_2])} => Reduce(v_1i, v_2i)
  std::optional<ReduceTag> tree_reduce_to_entry_reduce( ReduceTag, size_t i );
  // {Eval(Tree [v_1], Tree [v_2])} => Eval(v_1i, v_2i)
  std::optional<EvalTag> tree_eval_to_entry_eval( EvalTag, size_t i );

  CouponCollector( DeterministicRuntime& rt )
    : rt_( rt )
  {}
};
