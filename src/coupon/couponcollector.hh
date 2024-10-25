#pragma once

#include "handle.hh"
#include <optional>
#include <vector>

struct EquivalenceTag
{
  Handle<Fix> lhs;
  Handle<Fix> rhs;
};

struct EvalTag
{
  Handle<Fix> lhs;
  Handle<Value> rhs;
};

struct ApplyTag
{
  Handle<Tree> combination;
  Handle<Fix> result;
};

using CouponCollectorTag = std::variant<EquivalenceTag, EvalTag, ApplyTag>;

class DeterministicRuntime;
struct KernelExecutionTag;

class CouponCollector
{
  DeterministicRuntime& rt_;

public:
  // {} => Eval( x, x )
  EvalTag get_eval_identity( Handle<Blob> x );

  // {} => Equivalence( x, x )
  EquivalenceTag get_equivalence_reflex( Handle<Fix> x );
  // {Equivalence( x, y )} => Equivalence( y, x )
  EquivalenceTag get_equivalence_symmetry( EquivalenceTag );
  // {Equivalence( x, y ), Equivalence( y, z )}  => Equivalence( x, z )
  std::optional<EquivalenceTag> get_equivalence_transition( EquivalenceTag, EquivalenceTag );

  // {Equivalence( x, y ), Eval( y, z )}  => Eval( x, z )
  std::optional<EvalTag> equivalence_eval( EquivalenceTag, EvalTag );
  // {Equivalence( x, y ), Apply( y, z )}  => Apply( x, z )
  std::optional<ApplyTag> equivalence_apply( EquivalenceTag, ApplyTag );

  // {[Equivalence(v_i1, v_i2)]} => Equivalence([v_i1], [v_i2])
  EquivalenceTag tree_equivalence( std::vector<EquivalenceTag> tags );
  // {[Eval(v_i1, v_i2)]} => Eval([v_i1], [v_i2])
  EvalTag tree_eval( std::vector<EvalTag> tags );

  // KernelExecutionTag => ApplyTag
  std::optional<ApplyTag> execution_to_apply( KernelExecutionTag );
  // execution_to_apply( externref( Handle<Fix> ) )
  // {ApplyTag(Tree x, Thunk y)} => Equivalence( Thunk x, Thunk y )
  std::optional<EquivalenceTag> apply_to_thunk_equivalence( ApplyTag );
  // {ApplyTag(Tree x, y)} => Equivalence( Shallow(Thunk x), ref( y ) )
  std::optional<EquivalenceTag> apply_to_shallow_encode_equivalence( ApplyTag );
  // {ApplyTag(Tree x, y), Eval(y, z)} => Equivalence( Strict(Thunk x), z )
  std::optional<EvalTag> apply_to_strict_encode_equivalence( ApplyTag, EvalTag );
  // {Eval(Thunk x, y)} => Equivalence(Strict(Thunk x), y)
  std::optional<EquivalenceTag> eval_to_strict_encode_equivalence( EvalTag );

  CouponCollector( DeterministicRuntime& rt )
    : rt_( rt )
  {}
};
