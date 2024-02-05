#include <iostream>

#include "runtimes.hh"

using namespace std;

int min_args = 1;
int max_args = 2;

void program_body( std::span<char*> args )
{

  ios::sync_with_stdio( false );
  args = args.subspan( 1 );

  uint16_t port = 0;
  if ( not args.empty() ) {
    port = stoi( args[0] );
    args = args.subspan( 1 );
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
