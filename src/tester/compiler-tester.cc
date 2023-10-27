#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

#include "base16.hh"
#include "runtime.hh"
/* #include "tester-utils.hh" */

#include <glog/logging.h>

using namespace std;

int min_args = 2;
int max_args = 3;

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " file.wasm [label]\n";
}

void program_body( span_view<char*> args )
{
  ios::sync_with_stdio( false );

  args.remove_prefix( 1 ); // ignore argv[ 0 ]

  // make the combination from the given arguments
  // take the file.wasm argument
  const string_view str { args[0] };
  if ( not str.ends_with( ".wasm" ) ) {
    throw runtime_error( "unexpected non-wasm file" );
  }

  auto wasm = OwnedBlob::from_file( str );
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
  auto& rt = Runtime::get_instance();
  auto& storage = rt.storage();
  storage.deserialize();
  Handle blob = storage.add_blob( std::move( wasm ) );
  auto encode = OwnedMutTree::allocate( 3 );
  encode.at( 0 ) = Handle( "unused" );
  encode.at( 1 ) = storage.get_ref( "compile-encode" ).value();
  encode.at( 2 ) = blob;
  Handle encode_name = storage.add_tree( std::move( encode ) );
  Handle thunk_name = encode_name.as_thunk();
  Handle result = rt.eval( thunk_name );

  if ( ref_name ) {
    storage.set_ref( *ref_name, result );
  }
  Handle canonical = storage.canonicalize( result );
  storage.serialize( result );
  // print the result
  cout << "Result serialized to: " << base16::encode( canonical ) << "\n";
  if ( ref_name ) {
    cout << "Created ref: " << *ref_name << "\n";
  }
}
