#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "base64.hh"
#include "tester-utils.hh"

#define TRUSTED_COMPILE_ENCODE Handle( base64::decode( COMPILE_ENCODE ) )

using namespace std;
using path = filesystem::path;

void program_body( span_view<char*> args )
{
  auto& runtime = RuntimeStorage::get_instance();
  runtime.deserialize();
  ios::sync_with_stdio( false );

  args.remove_prefix( 1 );

  vector<tuple<string, int, int>> nodes;

  // format: username@server:cores:bps
  // e.g. username@stagecast.org/fix:256:100000
  nodes.emplace_back( "localhost", std::thread::hardware_concurrency(), 0 );

  while ( args.size() > 0 and args.at( 0 )[0] == '+' ) {
    string arg = args[0];
    args.remove_prefix( 1 );
    string server = arg.substr( 1, arg.find( ':' ) - 1 );
    arg = arg.substr( arg.find( ':' ) + 1 );
    string cores = arg.substr( 0, arg.find( ':' ) );
    string bps = arg.substr( arg.find( ':' ) + 1 );
    nodes.emplace_back( server, stoi( cores ), stoi( bps ) );
  }

  ReadOnlyFile scheduler_file( args[0] );

  Handle scheduler_blob = runtime.add_blob( string_view( scheduler_file ) );

  Tree scheduler_compile { 3 };
  scheduler_compile.at( 0 ) = Handle( "unused" );
  scheduler_compile.at( 1 ) = TRUSTED_COMPILE_ENCODE;
  scheduler_compile.at( 2 ) = scheduler_blob;
  Handle scheduler_compile_thunk = runtime.add_thunk( Thunk( runtime.add_tree( std::move( scheduler_compile ) ) ) );

  cerr << "Compiling Scheduler...\n";
  Handle compiled_scheduler = runtime.eval_thunk( scheduler_compile_thunk );
  cerr << "Compiled Scheduler.\n";
  runtime.set_scheduler( compiled_scheduler );

  args.remove_prefix( 1 );

  vector<ReadOnlyFile> open_files;

  Handle program_encode = parse_args( args, open_files );

  cerr << "Running Thunk:...\n";
  Handle result = runtime.eval_thunk( runtime.add_thunk( Thunk( program_encode ) ) );

  cout << "Result:\n" << pretty_print( result );
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

  try {
    if ( argc < 3 ) {
      usage_message( argv[0] );
      return EXIT_FAILURE;
    }

    span_view<char*> args = { argv, static_cast<size_t>( argc ) };
    program_body( args );
  } catch ( const exception& e ) {
    cerr << argv[0] << ": " << e.what() << "\n\n";
    usage_message( argv[0] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
