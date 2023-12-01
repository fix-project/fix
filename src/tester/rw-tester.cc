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
  cout << "Combination:\n" << deep_pretty_print( encode_name ) << "\n";

  auto& runtime = Runtime::get_instance();

  // make a Thunk that points to the combination
  Handle thunk_name = encode_name.as_thunk();

  // force the Thunk and print it
  Handle result = runtime.eval( thunk_name );

  // print the result
  cout << "Result:\n" << deep_pretty_print( result );

  runtime.serialize( Task::Eval( thunk_name ) );

  Handle canonical_thunk_name = runtime.storage().canonicalize( thunk_name );
  Handle canonical_result_name = runtime.storage().canonicalize( result );
  cout << "Eval: " << canonical_thunk_name << " --> " << canonical_result_name << endl;
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " entry...\n";
  cerr << "   entry :=   file:<filename>\n";
  cerr << "            | string:<string>\n";
  cerr << "            | name:<base16-encoded name>\n";
  cerr << "            | short-name:<prefix><first 7 bytes of a base16-encoded name> (with <prefix> = Blo(B) | "
          "Tre(E) | Thun(K) | Ta(G)\n";
  cerr << "            | uint<n>:<integer> (with <n> = 8 | 16 | 32 | 64)\n";
  cerr << "            | tree:<n> (followed by <n> entries)\n";
  cerr << "            | thunk: (followed by tree:<n>)\n";
  cerr << "            | compile:<filename>\n";
  cerr << "            | ref:<ref>\n";
}
