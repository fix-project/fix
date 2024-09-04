#include <iostream>
#include <unistd.h>

#include "handle.hh"
#include "runtimes.hh"
#include "tester-utils.hh"
#include "types.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  std::shared_ptr<Client> rt;

  if ( argc != 3 )
    cerr << "Usage: +[address]:[port] [label-to-compile]" << endl;

  if ( argv[1][0] == '+' ) {
    string addr( &argv[1][1] );
    if ( addr.find( ':' ) == string::npos ) {
      throw runtime_error( "invalid argument " + addr );
    }
    Address address( addr.substr( 0, addr.find( ':' ) ), stoi( addr.substr( addr.find( ':' ) + 1 ) ) );
    rt = Client::init( address );
  } else {
    exit( 1 );
  }

  auto compile_encode = rt->get_rt()
                          .labeled( "compile-encode" )
                          .unwrap<Expression>()
                          .unwrap<Object>()
                          .unwrap<Value>()
                          .unwrap<ValueTree>();

  auto target = rt->get_rt().labeled( argv[2] ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();

  auto application = OwnedMutTree::allocate( 3 );
  application[0] = make_limits( rt->get_rt(), 1024 * 1024 * 1024, 1024 * 1024, 1 );
  application[1] = Handle<Strict>( Handle<Identification>( compile_encode ) );
  application[2] = target;

  auto handle = rt->get_rt().create( make_shared<OwnedTree>( std::move( application ) ) ).unwrap<ValueTree>();

  auto res = rt->execute( Handle<Eval>( Handle<Object>( Handle<Application>( handle::upcast( handle ) ) ) ) );

  // print the result
  cout << "Result:\n" << res << endl;

  return 0;
}
