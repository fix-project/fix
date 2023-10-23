#include <charconv>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

#include "tester-utils.hh"

using namespace std;

int min_args = 2;
int max_args = -1;

void program_body( span_view<char*> args )
{
  ios::sync_with_stdio( false );
  vector<ReadOnlyFile> open_files;

  args.remove_prefix( 1 ); // ignore argv[ 0 ]

  // make the combination from the given arguments
  Handle encode_name = parse_args( args, open_files );

  if ( not args.empty() ) {
    throw runtime_error( "unexpected argument: "s + args.at( 0 ) );
  }

  // add the combination to the store, and print it
  cout << "Combination:\n" << pretty_print( encode_name ) << "\n";

  auto& runtime = Runtime::get_instance();

  // make a Thunk that points to the combination
  Handle thunk_name = runtime.storage().add_thunk( Thunk { encode_name } );

  // force the Thunk and print it
  Handle result = runtime.eval( thunk_name );

  // print the result
  cout << "Result:\n" << pretty_print( result );
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " entry...\n";
  cerr << "   entry :=   file:<filename>\n";
  cerr << "            | string:<string>\n";
  cerr << "            | name:<base64-encoded name>\n";
  cerr << "            | uint<n>:<integer> (with <n> = 8 | 16 | 32 | 64)\n";
  cerr << "            | tree:<n> (followed by <n> entries)\n";
  cerr << "            | thunk: (followed by tree:<n>)\n";
  cerr << "            | compile:<filename>\n";
  cerr << "            | ref:<ref>\n";
}
