#include <charconv>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

#define RUNTIME_THREADS 1

#include "tester-utils.hh"

using namespace std;

void program_body( span_view<char*> args )
{
  ios::sync_with_stdio( false );
  vector<ReadOnlyFile> open_files;

  args.remove_prefix( 1 ); // ignore argv[ 0 ]

  auto& runtime = Runtime::get_instance();

  while ( not args.empty() and args[0][0] == '+' ) {
    string addr( &args[0][1] );
    args.remove_prefix( 1 );
    if ( addr.find( ':' ) == string::npos ) {
      throw runtime_error( "invalid argument " + addr );
    }
    Address address( addr.substr( 0, addr.find( ':' ) ), stoi( addr.substr( addr.find( ':' ) + 1 ) ) );
    cout << "Connecting to remote " << address.to_string() << "\n";
    runtime.connect( address );
  }

  // make the combination from the given arguments
  Handle encode_name = parse_args( args, open_files );

  if ( not args.empty() ) {
    throw runtime_error( "unexpected argument: "s + args.at( 0 ) );
  }

  // add the combination to the store, and print it
  cout << "Combination:\n" << pretty_print( encode_name ) << endl;

  // make a Thunk that points to the combination
  Handle thunk_name = runtime.storage().add_thunk( Thunk { encode_name } );

  // Wait for the handshake
  sleep( 1 );

  // force the Thunk and print it
  Handle result = runtime.eval( thunk_name );

  // print the result
  cout << "Result:\n" << pretty_print( result );
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " [+server:port]... entry...\n";
  cerr << "   entry :=   file:<filename>\n";
  cerr << "            | string:<string>\n";
  cerr << "            | name:<base64-encoded name>\n";
  cerr << "            | uint<n>:<integer> (with <n> = 8 | 16 | 32 | 64)\n";
  cerr << "            | tree:<n> (followed by <n> entries)\n";
  cerr << "            | thunk: (followed by tree:<n>)\n";
  cerr << "            | compile:<filename>\n";
  cerr << "            | ref:<ref>\n";
}

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc < 2 ) {
    usage_message( argv[0] );
    return EXIT_FAILURE;
  }

  span_view<char*> args = { argv, static_cast<size_t>( argc ) };
  program_body( args );

  return EXIT_SUCCESS;
}
