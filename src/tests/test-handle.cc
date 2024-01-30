#include <cstdint>
#include <glog/logging.h>

#include "handle.hh"

using namespace std;

void test_nil( void )
{
  const u8x32 zero {};
  const auto nil = Handle<Fix>::forge( zero );
  const auto literal = nil.try_into<Expression>()
                         .and_then( []( const auto x ) { return x.template try_into<Object>(); } )
                         .and_then( []( const auto x ) { return x.template try_into<Value>(); } )
                         .and_then( []( const auto x ) { return x.template try_into<Blob>(); } )
                         .and_then( []( const auto x ) { return x.template try_into<Literal>(); } )
                         .value();
  CHECK_EQ( literal.size(), 0 );
  CHECK( not literal.is_local() );
}

void test_string_literal( void )
{
  const auto literal = "hello"_literal;
  const auto fix = Handle<Blob>( literal ).into<Value>().into<Object>().into<Expression>().into<Fix>();
  const auto extracted = fix.try_into<Expression>()
                           .value()
                           .try_into<Object>()
                           .value()
                           .try_into<Value>()
                           .value()
                           .try_into<Blob>()
                           .value()
                           .try_into<Literal>()
                           .value();

  CHECK_EQ( extracted, literal );
  CHECK_EQ( extracted.view(), "hello" );
  CHECK_EQ( string( extracted ), "hello" );
}

void test_int_literal( void )
{
  const auto literal = 0xcafeb0ba_literal32;
  const auto fix = Handle<Blob>( literal ).into<Value>().into<Object>().into<Expression>().into<Fix>();
  const auto extracted = fix.try_into<Expression>()
                           .value()
                           .try_into<Object>()
                           .value()
                           .try_into<Value>()
                           .value()
                           .try_into<Blob>()
                           .value()
                           .try_into<Literal>()
                           .value();
  CHECK_EQ( extracted, literal );
  CHECK_EQ( extracted.size(), sizeof( uint32_t ) );
  CHECK_EQ( uint32_t( extracted ), 0xcafeb0ba );
  CHECK_EQ( Handle<Literal>( 0xcafeb0ba ), extracted );
}

void test_ref( void )
{
  const auto nil = Handle<ValueTree>::nil();
  const auto ref = nil.into<ValueTreeRef>();
  CHECK_NE( Handle<Fix>( ref ), Handle<Fix>( nil ) );
  CHECK_EQ( nil.size(), 0 );
  CHECK( not nil.is_local() );
  CHECK_EQ( ref.unwrap<ValueTree>().size(), 0 );
  CHECK( not ref.unwrap<ValueTree>().is_local() );
}

void test_tag( void )
{
  const auto nil = Handle<Fix>( Handle<ValueTree>::nil() );
  const auto tree = nil.try_into<Expression>()
                      .value()
                      .try_into<Object>()
                      .value()
                      .try_into<Value>()
                      .value()
                      .try_into<ValueTree>()
                      .value();
  const auto tag = Handle<Value>( tree.tag() ).into<Object>().into<Expression>().into<Fix>();
  CHECK_NE( tag, nil );
  const auto inner = tag.try_into<Expression>()
                       .value()
                       .try_into<Object>()
                       .value()
                       .try_into<Value>()
                       .value()
                       .try_into<ValueTree>()
                       .value();
  CHECK( inner.is_tag() );
  CHECK( not inner.untag().is_tag() );
}

void test_thunks( void )
{
  const auto tree = Handle<ValueTree>::nil();
  const auto obj = Handle<Value>( tree );
  const auto combination = Handle<Application>( Handle<ExpressionTree>( tree ) );
  const auto id = Handle<Identification>( obj );

  CHECK_EQ( id.try_into<Value>().value(), obj );
  CHECK_EQ( id.try_into<Value>().value().try_into<ValueTree>().value(), tree );
  CHECK_EQ( combination.try_into<ExpressionTree>().value(), Handle<ExpressionTree>( tree ) );
}

void test_trees( void )
{
  const auto o = Handle<ValueTree>::nil();
  const auto v = Handle<ObjectTree>::nil();
  const auto e = Handle<ExpressionTree>::nil();

  const auto fo = Handle<Value>( o ).into<Object>().into<Expression>().into<Fix>();
  const auto fv = Handle<Object>( v ).into<Expression>().into<Fix>();
  const auto fe = Handle<Expression>( e ).into<Fix>();

  CHECK_NE( fo, fv );
  CHECK_NE( fo, fe );
  CHECK_NE( fv, fe );
}

template<FixType To, typename From>
Handle<To> check_conversion( Handle<From> x )
{
  return x;
}

void test_implicit_conversions( void )
{
  // We should be able to implicitly up-convert along any path not including a wrapper.
  // We can implicitly convert *to* a wrapper type, and we can implicitly convert *from* a wrapper type, but not
  // across one.

  auto literal = "foo"_literal;
  auto blob = check_conversion<Blob>( literal );
  static_assert( std::convertible_to<Handle<Literal>, Handle<Blob>> );
  check_conversion<Value>( literal );
  check_conversion<Object>( literal );
  check_conversion<Expression>( literal );
  auto fix_literal = check_conversion<Fix>( literal );
  auto fix_blob = check_conversion<Fix>( blob );
  auto ref = check_conversion<BlobRef>( literal );
  auto fix_ref = check_conversion<Fix>( ref );

  CHECK_EQ( fix_literal, fix_blob );
  CHECK_NE( fix_literal, fix_ref );

  CHECK( handle::kind( literal ) == FixKind::Value );
  CHECK( handle::kind( blob ) == FixKind::Value );
  CHECK( handle::kind( ref ) == FixKind::Value );
  CHECK( handle::kind( fix_literal ) == FixKind::Value );
  CHECK( handle::kind( fix_blob ) == FixKind::Value );
  CHECK( handle::kind( fix_ref ) == FixKind::Value );
}

void test_explicit_conversions( void )
{
  // We can explicitly up-convert between Trees.  Doing so, then up-converting to a Fix, should *not* produce the
  // same Handle as just up-converting to a Fix.

  auto vtree = Handle<ValueTree>::nil();
  auto otree = Handle<ObjectTree>( vtree );
  auto etree = Handle<ExpressionTree>( vtree );
  auto fix_vtree = check_conversion<Fix>( vtree );
  auto fix_etree = check_conversion<Fix>( etree );
  CHECK_NE( fix_vtree, fix_etree );

  auto object_vtree = check_conversion<Object>( vtree );
  auto fix_object_vtree = check_conversion<Fix>( object_vtree );
  auto fix_otree = check_conversion<Fix>( otree );
  CHECK_EQ( fix_object_vtree, fix_vtree );
  CHECK_NE( fix_otree, fix_object_vtree );

  CHECK( handle::kind( otree ) == FixKind::Object );
  CHECK( handle::kind( vtree ) == FixKind::Value );
  CHECK( handle::kind( etree ) == FixKind::Expression );

  CHECK( handle::kind( object_vtree ) == FixKind::Value );
  CHECK( handle::kind( fix_vtree ) == FixKind::Value );
  CHECK( handle::kind( fix_object_vtree ) == FixKind::Value );
  CHECK( handle::kind( fix_otree ) == FixKind::Object );
}

void test( void )
{
  test_nil();
  test_string_literal();
  test_int_literal();
  test_ref();
  test_tag();
  test_thunks();
  test_trees();
  test_implicit_conversions();
  test_explicit_conversions();
}
