#include "couponcollector.hh"
#include "runtime.hh"
#include <optional>

using namespace std;

EvalTag CouponCollector::get_eval_identity( Handle<Blob> x )
{
  EvalTag t { .lhs = x, .rhs = x };
  return t;
}

ReduceTag CouponCollector::get_reduce_identity( Handle<Blob> x )
{
  ReduceTag t { .lhs = x, .rhs = x };
  return t;
}

ReduceTag CouponCollector::get_reduce_identity( Handle<Thunk> x )
{
  ReduceTag t { .lhs = x, .rhs = x };
  return t;
}

ThinkTag CouponCollector::get_think_identity( Handle<Thunk> x )
{
  return ThinkTag { .thunk = x, .result = x };
}

EquivalenceTag CouponCollector::get_equivalence_reflexive( Handle<Fix> x )
{
  EquivalenceTag t { .lhs = x, .rhs = x };
  return t;
}

EquivalenceTag CouponCollector::get_equivalence_symmetric( EquivalenceTag x )
{
  EquivalenceTag t { .lhs = x.rhs, .rhs = x.lhs };
  return t;
}

optional<EquivalenceTag> CouponCollector::get_equivalence_transitive( EquivalenceTag t1, EquivalenceTag t2 )
{
  if ( equal( rt_.get_name( t1.rhs ), rt_.get_name( t2.lhs ) ) ) {
    EquivalenceTag t { .lhs = t1.lhs, .rhs = t2.rhs };
    return t;
  }

  return {};
}

optional<EvalTag> CouponCollector::equivalence_eval( EquivalenceTag equiv, EvalTag eval )
{
  if ( equal( rt_.get_name( equiv.rhs ), rt_.get_name( eval.lhs ) ) ) {
    EvalTag t { .lhs = equiv.lhs, .rhs = eval.rhs };
    return t;
  }

  return {};
}

optional<ReduceTag> CouponCollector::equivalence_reduce( EquivalenceTag equiv, ReduceTag reduce )
{
  if ( equal( rt_.get_name( equiv.rhs ), rt_.get_name( reduce.lhs ) ) ) {
    ReduceTag t { .lhs = equiv.lhs, .rhs = reduce.rhs };
    return t;
  }

  return {};
}

optional<ThinkTag> CouponCollector::equivalence_think( EquivalenceTag equiv, ThinkTag think )
{
  if ( equal( rt_.get_name( equiv.rhs ), rt_.get_name( think.thunk ) ) ) {
    ThinkTag t { .thunk = equiv.lhs.unwrap<Thunk>(), .result = think.result };
    return t;
  }

  return {};
}

optional<ThinkTag> CouponCollector::equivalence_thunk_result( EquivalenceTag equiv, ThinkTag think )
{
  if ( equal( rt_.get_name( equiv.rhs ), rt_.get_name( think.result ) ) ) {
    ThinkTag t { .thunk = think.thunk, .result = equiv.lhs };
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
    i++;
  }

  auto lhs_tree = rt_.create_tree( make_shared<OwnedTree>( std::move( lhs ) ) );
  auto rhs_tree = rt_.create_tree( make_shared<OwnedTree>( std::move( rhs ) ) );
  EvalTag t { .lhs = lhs_tree, .rhs = rhs_tree };
  return t;
}

ReduceTag CouponCollector::tree_reduce( std::vector<ReduceTag> tags )
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
  ReduceTag t { .lhs = lhs_tree, .rhs = rhs_tree };
  return t;
}

optional<ApplyTag> CouponCollector::execution_to_apply( KernelExecutionTag execution )
{
  // XXX: Check Runnable
  auto tree = rt_.attach( execution.combination );
  if ( equal( rt_.get_name( tree->at( 1 ) ), rt_.get_name( execution.procedure ) ) ) {
    ApplyTag t { .combination = execution.combination, .result = execution.result };
    return t;
  }

  return {};
}

SelectTag CouponCollector::select( Handle<Tree> combination )
{
  auto tree = rt_.attach( combination );
  return {};
}

IdentifyTag CouponCollector::identify( Handle<Value> value )
{
  IdentifyTag t { .combination = value, .result = value };
  return t;
}

optional<ThinkTag> CouponCollector::apply_to_think( ReduceTag reduce, ApplyTag apply )
{
  if ( equal( rt_.get_name( reduce.rhs ), rt_.get_name( apply.combination ) ) && !rt_.is_encode( apply.result ) ) {
    ThinkTag t { .thunk
                 = rt_.create_application_thunk( reduce.lhs.unwrap<Value>().unwrap<Treeish>().unwrap<Tree>() ),
                 .result = apply.result };
    return t;
  }

  return {};
}

optional<ThinkTag> CouponCollector::select_to_think( ReduceTag reduce, SelectTag select )
{
  if ( equal( rt_.get_name( reduce.rhs ), rt_.get_name( select.combination ) ) ) {
    ThinkTag t { .thunk = rt_.create_selection_thunk( reduce.lhs.unwrap<Value>().unwrap<Treeish>().unwrap<Tree>() ),
                 .result = select.result };
    return t;
  }

  return {};
}

optional<ThinkTag> CouponCollector::identify_to_think( ReduceTag reduce, IdentifyTag identify )
{
  if ( equal( rt_.get_name( reduce.rhs ), rt_.get_name( identify.combination ) ) ) {
    ThinkTag t { .thunk = rt_.create_selection_thunk( reduce.lhs.unwrap<Value>().unwrap<Treeish>().unwrap<Tree>() ),
                 .result = identify.result };
    return t;
  }

  return {};
}

optional<ThinkTag> CouponCollector::think_transitive( ThinkTag t1, ThinkTag t2 )
{
  if ( equal( rt_.get_name( t1.result ), rt_.get_name( t2.thunk ) ) ) {
    ThinkTag t { .thunk = t1.thunk, .result = t2.result };
    return t;
  }

  return {};
}

optional<EquivalenceTag> CouponCollector::think_to_thunk_equivalence( ThinkTag think )
{
  if ( rt_.is_thunk( think.result ) ) {
    EquivalenceTag t { .lhs = think.thunk, .rhs = think.result.unwrap<Thunk>() };
    return t;
  }

  return {};
}

optional<ReduceTag> CouponCollector::think_to_shallow_encode_reduce( ThinkTag think )
{
  if ( !rt_.is_thunk( think.result ) ) {
    ReduceTag t { .lhs = rt_.create_shallow_encode( think.thunk ), .rhs = think.result };
    return t;
  }

  return {};
}

optional<ReduceTag> CouponCollector::think_to_strict_encode_reduce( ThinkTag think, EvalTag eval )
{
  if ( equal( rt_.get_name( think.result ), rt_.get_name( eval.lhs ) ) ) {
    ReduceTag t { .lhs = rt_.create_strict_encode( think.thunk ), .rhs = think.result };
    return t;
  }

  return {};
}

optional<EvalTag> CouponCollector::think_to_eval_thunk( ThinkTag think, EvalTag eval )
{
  if ( equal( rt_.get_name( think.result ), rt_.get_name( eval.lhs ) ) ) {
    EvalTag t { .lhs = think.thunk, .rhs = eval.rhs };
    return t;
  }

  return {};
}

optional<ReduceTag> CouponCollector::eval_thunk_to_strict_encode_reduce( EvalTag eval )
{
  if ( rt_.is_thunk( eval.lhs ) ) {
    ReduceTag t { .lhs = rt_.create_shallow_encode( eval.lhs.unwrap<Thunk>() ), .rhs = eval.rhs };
    return t;
  }

  return {};
}
