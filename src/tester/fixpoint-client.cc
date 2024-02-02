#include <iostream>
#include <unistd.h>

#include "handle_post.hh"
#include "runtimes.hh"
#include "tester-utils.hh"

using namespace std;

int min_args = 2;
int max_args = -1;

void program_body( std::span<char*> argspan )
{
  span_view<char*> args = { argspan.data(), argspan.size() };

  ios::sync_with_stdio( false );
  args.remove_prefix( 1 );

  std::shared_ptr<Client> client;

  if ( not args.empty() and args[0][0] == '+' ) {
    string addr( &args[0][1] );
    args.remove_prefix( 1 );
    if ( addr.find( ':' ) == string::npos ) {
      throw runtime_error( "invalid argument " + addr );
    }
    Address address( addr.substr( 0, addr.find( ':' ) ), stoi( addr.substr( addr.find( ':' ) + 1 ) ) );
    client = Client::init( address );
  } else {
    exit( 1 );
  }

  // make the combination from the given arguments
  auto handle = parse_args( *client, args );

  if ( not args.empty() ) {
    throw runtime_error( "unexpected argument: "s + args.at( 0 ) );
  }

  if ( !handle::extract<Object>( handle ).has_value() ) {
    cerr << "Handle is not an Object" << endl;
  }

  auto res = client->execute( Handle<Eval>( handle::extract<Object>( handle ).value() ) );

  // print the result
  cout << "Result:\n" << res << endl;
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " +address:port entries...\n";
  parser_usage_message();
}
