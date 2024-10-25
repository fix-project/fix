#include "couponcollector.hh"
#include "runtime.hh"
#include <optional>

using namespace std;

EvalTag CouponCollector::get_eval_identity( Handle<Blob> x )
{
  EvalTag t { .lhs = x, .rhs = x };
  return t;
}

EquivalenceTag CouponCollector::get_equivalence_reflex( Handle<Fix> x )
{
  EquivalenceTag t { .lhs = x, .rhs = x };
  return t;
}

EquivalenceTag CouponCollector::get_equivalence_symmetry( EquivalenceTag x )
{
  EquivalenceTag t { .lhs = x.rhs, .rhs = x.lhs };
  return t;
}

optional<EquivalenceTag> CouponCollector::get_equivalence_transition( EquivalenceTag t1, EquivalenceTag t2 )
{
  if ( equal( rt_.get_name( t1.rhs ), rt_.get_name( t2.lhs ) ) ) {
    EquivalenceTag t { .lhs = t1.lhs, .rhs = t2.rhs };
    return t;
  }

  return {};
}

optional<EvalTag> CouponCollector::equivalence_eval( EquivalenceTag equiv, EvalTag eval )
{
  if ( equal( rt_.get_name( equiv.lhs ), rt_.get_name( eval.lhs ) ) ) {
    EvalTag t { .lhs = equiv.rhs, .rhs = eval.rhs };
    return t;
  }

  return {};
}

optional<ApplyTag> CouponCollector::equivalence_apply( EquivalenceTag equiv, ApplyTag apply )
{
  if ( equal( rt_.get_name( equiv.lhs ), rt_.get_name( apply.combination ) ) ) {
    ApplyTag t { .combination = equiv.rhs.unwrap<Value>().unwrap<Treeish>().unwrap<Tree>(),
                 .result = apply.result };
    return t;
  }

  return {};
}

EquivalenceTag CouponCollector::tree_equivalence( std::vector<EquivalenceTag> tags )
{
  auto lhs = OwnedMutTree::allocate( tags.size() );
  auto rhs = OwnedMutTree::allocate( tags.size() );

  size_t i = 0;
  for ( const auto& t : tags ) {
    lhs.at( i ) = t.lhs;
    rhs.at( i ) = t.rhs;
    i++;
  }

  auto lhs_tree = rt_.create_tree( make_shared<OwnedTree>( std::move( lhs ) ) );
  auto rhs_tree = rt_.create_tree( make_shared<OwnedTree>( std::move( rhs ) ) );
  EquivalenceTag t { .lhs = lhs_tree, .rhs = rhs_tree };
  return t;
}

EvalTag CouponCollector::tree_eval( std::vector<EvalTag> tags )
{
  auto lhs = OwnedMutTree::allocate( tags.size() );
  auto rhs = OwnedMutTree::allocate( tags.size() );

  size_t i = 0;
  for ( const auto& t : tags ) {
    lhs.at( i ) = t.lhs;
    rhs.at( i ) = t.rhs;
  }

  auto lhs_tree = rt_.create_tree( make_shared<OwnedTree>( std::move( lhs ) ) );
  auto rhs_tree = rt_.create_tree( make_shared<OwnedTree>( std::move( rhs ) ) );
  EvalTag t { .lhs = lhs_tree, .rhs = rhs_tree };
  return t;
}

optional<ApplyTag> CouponCollector::execution_to_apply( KernelExecutionTag execution )
{
  // XXX: Check Runnable
  // XXX: Check combination does not contain encode
  auto tree = rt_.attach( execution.combination );
  if ( equal( rt_.get_name( tree->at( 1 ) ), rt_.get_name( execution.procedure ) ) ) {
    ApplyTag t { .combination = execution.combination, .result = execution.result };
    return t;
  }

  return {};
}

optional<EquivalenceTag> CouponCollector::apply_to_thunk_equivalence( ApplyTag apply )
{
  if ( rt_.is_thunk( apply.result ) ) {
    EquivalenceTag t { .lhs = rt_.create_application_thunk( apply.combination ), .rhs = apply.result };
    return t;
  }

  return {};
}

optional<EquivalenceTag> CouponCollector::apply_to_shallow_encode_equivalence( ApplyTag apply )
{
  if ( !rt_.is_thunk( apply.result ) ) {
    EquivalenceTag t { .lhs = rt_.create_shallow_encode( rt_.create_application_thunk( apply.combination ) ),
                       .rhs = rt_.ref( apply.result.unwrap<Value>() ) };
    return t;
  }

  return {};
}

optional<EvalTag> CouponCollector::apply_to_strict_encode_equivalence( ApplyTag apply, EvalTag eval )
{
  if ( equal( rt_.get_name( apply.result ), rt_.get_name( eval.lhs ) ) ) {
    EvalTag t { .lhs = rt_.create_strict_encode( rt_.create_application_thunk( apply.combination ) ),
                .rhs = eval.rhs };
    return t;
  }

  return {};
}

optional<EquivalenceTag> CouponCollector::eval_to_strict_encode_equivalence( EvalTag eval )
{
  if ( rt_.is_thunk( eval.lhs ) ) {
    EquivalenceTag t { .lhs = rt_.create_strict_encode( eval.lhs.unwrap<Thunk>() ), .rhs = eval.rhs };
    return t;
  }

  return {};
}
