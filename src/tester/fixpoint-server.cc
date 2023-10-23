#include <iostream>

#include "runtime.hh"
#include "tester-utils.hh"

using namespace std;

int min_args = 1;
int max_args = 2;

#include <glog/logging.h>

void program_body( span_view<char*> args )
{
  ios::sync_with_stdio( false );
  vector<ReadOnlyFile> open_files;

  args.remove_prefix( 1 ); // ignore argv[ 0 ]

  uint16_t port = 0;
  if ( not args.empty() ) {
    port = stoi( args[0] );
    args.remove_prefix( 1 );
  }

  Address address( "0.0.0.0", port );

  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  Address listen_address = rt.start_server( address );
  cout << "Listening on " << listen_address.to_string() << endl;
  while ( true )
    ;
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " [port]\n";
}
