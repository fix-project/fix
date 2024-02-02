#include <iostream>

#include "runtimes.hh"

using namespace std;

int min_args = 1;
int max_args = 2;

void program_body( std::span<char*> argspan )
{
  span_view<char*> args = { argspan.data(), argspan.size() };

  ios::sync_with_stdio( false );
  args.remove_prefix( 1 ); // ignore argv[ 0 ]

  uint16_t port = 0;
  if ( not args.empty() ) {
    port = stoi( args[0] );
    args.remove_prefix( 1 );
  }

  Address address( "0.0.0.0", port );
  auto server = Server::init( address );

  while ( true )
    ;
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " [port]\n";
}
