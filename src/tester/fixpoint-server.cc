#include <iostream>
#include <memory>
#include <stdexcept>

#include "mmap.hh"
#include "option-parser.hh"
#include "runtimes.hh"
#include "scheduler.hh"

using namespace std;

void split( const string_view str, const char ch_to_find, vector<string_view>& ret )
{
  ret.clear();

  unsigned int field_start = 0;
  for ( unsigned int i = 0; i < str.size(); i++ ) {
    if ( str[i] == ch_to_find ) {
      ret.emplace_back( str.substr( field_start, i - field_start ) );
      field_start = i + 1;
    }
  }

  ret.emplace_back( str.substr( field_start ) );
}

int main( int argc, char* argv[] )
{
  OptionParser parser( "fixpoint-server", "Run a fixpoint server" );
  uint16_t port;
  optional<const char*> local;
  optional<const char*> peerfile;
  optional<string> sche_opt;
  parser.AddArgument(
    "listening-port", OptionParser::ArgumentCount::One, [&]( const char* argument ) { port = stoi( argument ); } );
  parser.AddOption( 'a',
                    "address",
                    "address",
                    "Address of this server. This address does not change the listening address, and is used "
                    "solely for distinguishing this server from the list of peer servers.",
                    [&]( const char* argument ) { local = argument; } );
  parser.AddOption(
    'p', "peers", "peers", "Path to a file that contains a list of all servers.", [&]( const char* argument ) {
      peerfile = argument;
    } );
  parser.AddOption(
    's', "scheduler", "scheduler", "Scheduler to use [local, onepass, hint, random]", [&]( const char* argument ) {
      sche_opt = argument;
      if ( not( *sche_opt == "local" or *sche_opt == "onepass" or *sche_opt == "hint" or *sche_opt == "random" ) ) {
        throw runtime_error( "Invalid scheduler: " + sche_opt.value() );
      }
    } );
  parser.Parse( argc, argv );

  Address listen_address( "0.0.0.0", port );
  vector<Address> peer_address;

  if ( local.has_value() and peerfile.has_value() ) {
    Address local_address( local.value(), port );

    ReadOnlyFile peers( peerfile.value() );
    vector<string_view> ret;
    split( peers, '\n', ret );

    for ( const auto p : ret ) {
      if ( p.empty() )
        continue;
      if ( p.find( ':' ) == string::npos ) {
        throw runtime_error( "Invalid peer address " + string( p ) );
      }

      Address addr( string( p.substr( 0, p.find( ':' ) ) ), stoi( string( p.substr( p.find( ':' ) + 1 ) ) ) );

      if ( addr == local_address ) {
        peer_address.push_back( listen_address );
      } else {
        peer_address.push_back( addr );
      }
    }
  }

  shared_ptr<Scheduler> scheduler = make_shared<LocalFirstScheduler>();
  if ( sche_opt.has_value() ) {
    if ( *sche_opt == "onepass" ) {
      scheduler = make_shared<OnePassScheduler>();
    } else if ( *sche_opt == "hint" ) {
      scheduler = make_shared<HintScheduler>();
    } else if ( *sche_opt == "random" ) {
      scheduler = make_shared<RandomScheduler>();
    }
  }

  auto server = Server::init( listen_address, scheduler, peer_address );
  cout << "Server initialized" << endl;

  while ( true )
    ;

  return 0;
}
