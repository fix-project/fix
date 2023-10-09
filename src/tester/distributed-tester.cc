#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "base64.hh"
#include "socket.hh"
#include "tester-utils.hh"

#include "channel.hh"

using namespace std;
using path = filesystem::path;

void program_body( span_view<char*> args )
{
  auto& runtime = RuntimeStorage::get_instance();
  runtime.deserialize();
  ios::sync_with_stdio( false );

  args.remove_prefix( 1 );

  ReadOnlyFile scheduler_file( args[0] );

  cerr << "Listening on " << runtime.start_server( { "0.0.0.0", 26232 } ).to_string() << "\n";

  Handle scheduler_blob = runtime.add_blob( string_view( scheduler_file ) );

  Tree scheduler_compile { 3 };
  scheduler_compile.at( 0 ) = Handle( "unused" );
  scheduler_compile.at( 1 ) = COMPILE_ENCODE;
  scheduler_compile.at( 2 ) = scheduler_blob;
  Handle scheduler_compile_thunk = runtime.add_thunk( Thunk( runtime.add_tree( std::move( scheduler_compile ) ) ) );

  cerr << "Compiling Scheduler...\n";
  Handle compiled_scheduler = runtime.eval_thunk( scheduler_compile_thunk );
  cerr << "Compiled Scheduler.\n";
  runtime.set_scheduler( compiled_scheduler );

  args.remove_prefix( 1 );

  vector<ReadOnlyFile> open_files;

  Handle program_encode = parse_args( args, open_files );

  Handle thunk = runtime.add_thunk( Thunk( program_encode ) );

  Handle serialized = base64::decode( runtime.serialize( thunk ) );
  cerr << "Program Thunk: " << base64::encode( serialized ) << "\n";
  runtime.set_ref( "last-thunk", serialized );
  runtime.serialize( serialized );
  while ( true ) {
    sleep( 1 );
  }

  /*   cerr << "Running Thunk:...\n"; */
  /*   Handle result = runtime.eval_thunk( runtime.add_thunk( Thunk( program_encode ) ) ); */

  /*   cout << "Result:\n" << pretty_print( result ); */
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " +user@server:cores:bps... scheduler entry...\n";
  cerr << "   scheduler: a filename of a wasm blob corresponding to a valid scheduler program\n";
  cerr << "   entry :=   file:<filename>\n";
  cerr << "            | string:<string>\n";
  cerr << "            | name:<base64-encoded name>\n";
  cerr << "            | uint<n>:<integer> (with <n> = 8 | 16 | 32 | 64)\n";
  cerr << "            | tree:<n> (followed by <n> entries)\n";
  cerr << "            | thunk: (followed by tree:<n>)\n";
}

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc < 3 ) {
    usage_message( argv[0] );
    return EXIT_FAILURE;
  }

  span_view<char*> args = { argv, static_cast<size_t>( argc ) };
  program_body( args );

  return EXIT_SUCCESS;
}
