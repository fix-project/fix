#include <iostream>
#include <unistd.h>

#include "handle_post.hh"
#include "runtimes.hh"
#include "tester-utils.hh"

using namespace std;

int min_args = 0;
int max_args = -1;

int main( int argc, char* argv[] )
{
  std::shared_ptr<Client> client;

  if ( argc != 2 ) {
    cerr << "Usage: +[address]:[port]" << endl;
    exit( 1 );
  }

  if ( argv[1][0] == '+' ) {
    string addr( &argv[1][1] );
    if ( addr.find( ':' ) == string::npos ) {
      throw runtime_error( "invalid argument " + addr );
    }
    Address address( addr.substr( 0, addr.find( ':' ) ), stoi( addr.substr( addr.find( ':' ) + 1 ) ) );
    client = Client::init( address );
  } else {
    exit( 1 );
  }
  auto& rt = client->get_rt();

  auto mapreduce = rt.labeled( "mapreduce" ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();
  auto mapper = rt.labeled( "count-words" ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();
  auto reducer = rt.labeled( "merge-counts" ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();
  auto wikipedia = rt.labeled( "wikipedia" ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();

  auto application = OwnedMutTree::allocate( 6 );
  application[0] = make_limits( rt, 1024 * 1024 * 1024, 1024 * 1024, 1 );
  application[1] = mapreduce;
  application[2] = mapper;
  application[3] = reducer;
  application[4] = wikipedia;
  application[5] = make_limits( rt, 1024 * 1024 * 1024, 256 * 8, 1 );

  auto handle = rt.create( make_shared<OwnedTree>( std::move( application ) ) ).unwrap<ValueTree>();
  auto res = client->execute( Handle<Eval>( Handle<Object>( Handle<Application>( handle::upcast( handle ) ) ) ) );
  /* auto res = client->execute( Handle<Eval>( Handle<Identification>( wikipedia ) ) ); */

  // print the result
  cerr << "Result:\n" << res << endl;
  cerr << "Result:\n" << res.content << endl;

  auto named = res.unwrap<Blob>().unwrap<Named>();
  auto result = client->get_rt().get( named ).value();

  if ( std::filesystem::exists( "wikipedia.out" ) ) {
    std::filesystem::remove( "wikipedia.out" );
  }
  result->to_file( "wikipedia.out" );

  return 0;
}
