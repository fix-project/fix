#include "test.hh"
#include <sys/wait.h>

using namespace std;

Address address( "0.0.0.0", 12345 );

uint32_t fix_add( uint32_t a, uint32_t b, Handle add_elf )
{
  Handle add = thunk( tree( { blob( "unused" ), add_elf, a, b } ) );
  auto& rt = Runtime::get_instance();
  (void)a, (void)b;
  Handle result = rt.eval( add );
  uint32_t x = -1;
  memcpy( &x, rt.storage().get_blob( result ).data(), sizeof( uint32_t ) );
  return x;
}

void check_add( uint32_t a, uint32_t b, Handle add, string name )
{
  uint32_t sum_blob = fix_add( a, b, add );
  printf( "%s: %u + %u = %u\n", name.c_str(), a, b, sum_blob );
  if ( sum_blob != a + b ) {
    fprintf( stderr, "%s: got %u + %u = %u, expected %u.\n", name.c_str(), a, b, sum_blob, a + b );
    exit( 1 );
  }
}

void client()
{
  auto& rt = Runtime::get_instance( 1 );
  rt.storage().deserialize();
  cout << "Client connecting " << endl;
  rt.connect( address );

  Handle addblob = compile( file( "testing/wasm-examples/addblob.wasm" ) );
  rt.eval( addblob );
  Handle add_simple = compile( file( "testing/wasm-examples/add-simple.wasm" ) );
  rt.eval( add_simple );

  check_add( 7, 8, addblob, "addblob" );

  cout << "Client done" << endl;
}

void server( int client_pid )
{
  (void)client_pid;
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  rt.start_server( address );

  while ( kill( client_pid, 0 ) == 0 ) {
    sleep( 1 );
  }
}

void test( void )
{
  auto client_pid = fork();
  if ( client_pid ) {
    auto server_pid = fork();
    if ( server_pid ) {
      waitpid( client_pid, NULL, 0 );
      waitpid( server_pid, NULL, 0 );
    } else {
      server( client_pid );
      exit( 0 );
    }
  } else {
    sleep( 1 );
    client();
    exit( 0 );
  }
}
