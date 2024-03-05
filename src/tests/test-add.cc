#include <stdexcept>
#include <stdio.h>

#include "relater.hh"
#include "test.hh"

using namespace std;

auto rt = make_shared<Relater>();

uint32_t fix_add( uint32_t a, uint32_t b, Handle<Fix> add_elf )
{
  auto combination = tree( *rt,
                           blob( *rt, "" ).into<Fix>(),
                           add_elf,
                           Handle<Literal>( a ).into<Fix>(),
                           Handle<Literal>( b ).into<Fix>() )
                       .visit<Handle<ExpressionTree>>( []( auto h ) { return Handle<ExpressionTree>( h ); } );
  auto add = Handle<Application>( combination );
  (void)a, (void)b;
  auto result = rt->execute( Handle<Eval>( add ) );
  uint32_t x = -1;

  auto res = result.try_into<Blob>().and_then( []( auto h ) { return h.template try_into<Literal>(); } );
  if ( res ) {
    memcpy( &x, res->data(), sizeof( uint32_t ) );
  } else {
    throw runtime_error( "Invalid add result." );
  }
  return x;
}

void check_add( uint32_t a, uint32_t b, Handle<Fix> add_elf, string name )
{
  uint32_t sum_blob = fix_add( a, b, add_elf );
  printf( "%s: %u + %u = %u\n", name.c_str(), a, b, sum_blob );
  if ( sum_blob != a + b ) {
    fprintf( stderr, "%s: got %u + %u = %u, expected %u.\n", name.c_str(), a, b, sum_blob, a + b );
    exit( 1 );
  }
}

void check_add( uint32_t a, uint32_t b )
{
  static Handle<Fix> addblob = compile( *rt, file( *rt, "testing/wasm-examples/addblob.wasm" ) );
  static Handle<Fix> add_simple = compile( *rt, file( *rt, "testing/wasm-examples/add-simple.wasm" ) );
  check_add( a, b, addblob, "addblob" );
  check_add( a, b, add_simple, "add-simple" );
}

void test( void )
{
  for ( size_t i = 0; i < 32; i++ ) {
    check_add( random(), random() );
  }
  check_add( 0, 0 );
  check_add( pow( 2, 32 ) - 5, pow( 2, 31 ) + 12 );
}
