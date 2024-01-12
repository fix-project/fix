#include <cstdint>
#include <glog/logging.h>

#include "handle.hh"

using namespace std;

void test_nil( void )
{
  const u8x32 zero {};
  const auto nil = Handle<Fix>::forge( zero );
  const auto name = nil.unwrap<Expression>().unwrap<Value>().unwrap<Object>().unwrap<ObjectTree>();
  CHECK_EQ( name.size(), 0 );
  CHECK( not name.is_local() );
}

void test_string_literal( void )
{
  const auto literal = "hello"_literal;
  const auto fix = Handle<Blob>( literal ).into<Object>().into<Value>().into<Expression>().into<Fix>();
  const auto extracted = fix.unwrap<Expression>().unwrap<Value>().unwrap<Object>().unwrap<Blob>().unwrap<Literal>();

  CHECK_EQ( extracted, literal );
  CHECK_EQ( extracted.view(), "hello" );
  CHECK_EQ( string( extracted ), "hello" );
}

void test_int_literal( void )
{
  const auto literal = 0xcafeb0ba_literal32;
  const auto fix = Handle<Blob>( literal ).into<Object>().into<Value>().into<Expression>().into<Fix>();
  const auto extracted = fix.unwrap<Expression>().unwrap<Value>().unwrap<Object>().unwrap<Blob>().unwrap<Literal>();
  CHECK_EQ( extracted, literal );
  CHECK_EQ( extracted.size(), sizeof( uint32_t ) );
  CHECK_EQ( uint32_t( extracted ), 0xcafeb0ba );
  CHECK_EQ( Handle<Literal>( 0xcafeb0ba ), extracted );
}

void test_stub( void )
{
  const u8x32 zero { 0 };
  const auto nil = Handle<Fix>::forge( zero );
  const auto tree = nil.unwrap<Expression>().unwrap<Value>().unwrap<Object>().unwrap<ObjectTree>();
  const auto stub = Handle<ObjectTreeStub>( Handle<ExpressionTree>( tree ) )
                      .into<Object>()
                      .into<Value>()
                      .into<Expression>()
                      .into<Fix>();
  CHECK_NE( stub, nil );
  CHECK_EQ( tree.size(), 0 );
  CHECK( not tree.is_local() );
}

void test_tag( void )
{
  const u8x32 zero { 0 };
  const auto nil = Handle<Fix>::forge( zero );
  const auto tree = nil.unwrap<Expression>().unwrap<Value>().unwrap<Object>().unwrap<ObjectTree>();
  const auto tag = Handle<Object>( tree.tag() ).into<Value>().into<Expression>().into<Fix>();
  CHECK_NE( tag, nil );
  const auto inner = tag.unwrap<Expression>().unwrap<Value>().unwrap<Object>().unwrap<ObjectTree>();
  CHECK( inner.is_tag() );
  CHECK( not inner.untag().is_tag() );
}

void test_thunks( void )
{
  const auto tree = Handle<ObjectTree>::nil();
  const auto obj = Handle<Object>( tree );
  const auto combination = Handle<Combination>( Handle<ExpressionTree>( tree ) );
  const auto id = Handle<Identity>( obj );

  CHECK_EQ( id.unwrap<Object>(), obj );
  CHECK_EQ( id.unwrap<Object>().unwrap<ObjectTree>(), tree );
  CHECK_EQ( combination.unwrap<ExpressionTree>(), Handle<ExpressionTree>( tree ) );
}

void test_trees( void )
{
  const auto o = Handle<ObjectTree>::nil();
  const auto v = Handle<ValueTree>::nil();
  const auto e = Handle<ExpressionTree>::nil();
  const auto f = Handle<FixTree>::nil();

  const auto fo = Handle<Object>( o ).into<Value>().into<Expression>().into<Fix>();
  const auto fv = Handle<Value>( v ).into<Expression>().into<Fix>();
  const auto fe = Handle<Expression>( e ).into<Fix>();
  const auto ff = Handle<Fix>( f );

  CHECK_NE( fo, fv );
  CHECK_NE( fo, fe );
  CHECK_NE( fo, ff );
  CHECK_NE( fv, fe );
  CHECK_NE( fv, ff );
  CHECK_NE( fe, ff );
}

void test( void )
{
  test_nil();
  test_string_literal();
  test_int_literal();
  test_stub();
  test_tag();
  test_thunks();
  test_trees();
}
