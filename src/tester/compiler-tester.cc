#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

#include "base64.hh"
#include "tester-utils.hh"

using namespace std;

void program_body( span_view<char*> args )
{
  ios::sync_with_stdio( false );
  vector<ReadOnlyFile> open_files;

  args.remove_prefix( 1 ); // ignore argv[ 0 ]

  // make the combination from the given arguments
  // take the file.wasm argument
  const string_view str { args[0] };
  if ( not str.ends_with( ".wasm" ) ) {
    throw runtime_error( "unexpected non-wasm file" );
  }

  open_files.emplace_back( string( str ) );
  args.remove_prefix( 1 );

  optional<string> ref_name {};
  if ( not args.empty() ) {
    ref_name = args[0];
    args.remove_prefix( 1 );
  }

  if ( not args.empty() ) {
    throw runtime_error( "unexpected argument: "s + args.at( 0 ) );
  }

  // construct an encode for compiling the file
  auto& runtime = RuntimeStorage::get_instance();
  runtime.deserialize();
  Handle blob = runtime.add_blob( static_cast<string_view>( open_files.back() ) );
  Tree encode { 3 };
  encode.at( 0 ) = Handle( "unused" );
  encode.at( 1 ) = COMPILE_ENCODE;
  encode.at( 2 ) = blob;
  Handle encode_name = runtime.add_tree( std::move( encode ) );
  // make a Thunk that points to the combination
  Handle thunk_name = runtime.add_thunk( Thunk { encode_name } );

  // force the Thunk
  Handle result = runtime.eval_thunk( thunk_name );

  if ( ref_name ) {
    runtime.set_ref( *ref_name, result );
  }
  string serialized_result = runtime.serialize( result );
  // print the result
  cout << "Result:\n" << pretty_print( result );
  cout << "Result serialized to: " << serialized_result << "\n";
  if ( ref_name ) {
    cout << "Created ref: " << *ref_name << "\n";
  }
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " file.wasm [label]\n";
}

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  try {
    if ( argc < 2 ) {
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
