#include <iostream>

#include "runtime.hh"
#include "tester-utils.hh"

using namespace std;

void program_body( span_view<char*> args )
{
  ios::sync_with_stdio( false );
  vector<ReadOnlyFile> open_files;

  args.remove_prefix( 1 ); // ignore argv[ 0 ]

  uint16_t port = 0;
  if ( not args.empty() ) {
    port = stoi( args[0] );
    args.remove_prefix( 1 );
  }

  Address address( "0.0.0.0", port );

  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  Address listen_address = rt.start_server( address );
  cout << "Listening on " << listen_address.to_string() << endl;
  while ( true )
    ;
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " [port]\n";
}

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc > 2 ) {
    usage_message( argv[0] );
    return EXIT_FAILURE;
  }

  span_view<char*> args = { argv, static_cast<size_t>( argc ) };
  program_body( args );

  return EXIT_SUCCESS;
}
