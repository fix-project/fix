#include "tagger.hh"
#include "couponcollector.hh"
#include "operators.hh"
#include <optional>

using namespace std;

optional<EquivalenceTag> Tagger::exchange_tag( Handle<Tag> x, EquivalenceTag equiv )
{
  if ( content_equal( rt_.get_name( x ), rt_.get_name( equiv.lhs ) ) && rt_.is_tree( equiv.lhs ) ) {
    EquivalenceTag t {
      .lhs = x,
      .rhs = rt_.create_arbitrary_tag( rt_.attach( equiv.rhs.unwrap<Value>().unwrap<Treeish>().unwrap<Tree>() ) ) };
    return t;
  }

  return {};
}

optional<EvalTag> Tagger::exchange_tag( Handle<Tag> x, EvalTag eval )
{
  if ( content_equal( rt_.get_name( x ), rt_.get_name( eval.lhs ) ) && rt_.is_tree( eval.lhs ) ) {
    EvalTag t {
      .lhs = x,
      .rhs = rt_.create_arbitrary_tag( rt_.attach( eval.rhs.unwrap<Value>().unwrap<Treeish>().unwrap<Tree>() ) ) };
    return t;
  }

  return {};
}

optional<ReduceTag> Tagger::exchange_tag( Handle<Tag> x, ReduceTag reduce )
{
  if ( content_equal( rt_.get_name( x ), rt_.get_name( reduce.lhs ) ) && rt_.is_tree( reduce.lhs ) ) {
    ReduceTag t { .lhs = x,
                  .rhs = rt_.create_arbitrary_tag(
                    rt_.attach( reduce.rhs.unwrap<Value>().unwrap<Treeish>().unwrap<Tree>() ) ) };
    return t;
  }

  return {};
}
