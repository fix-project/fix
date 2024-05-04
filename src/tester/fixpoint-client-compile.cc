#include <iostream>
#include <unistd.h>

#include "base16.hh"
#include "handle.hh"
#include "runtimes.hh"
#include "tester-utils.hh"
#include "types.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  std::shared_ptr<Client> rt;

  if ( argc != 2 )
    cerr << "Usage: +[address]:[port]" << endl;

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
  rt->get_rt().get( compile_encode );

  auto wasm2c = Handle<BlobRef>(
    Handle<Fix>::forge( base16::decode( "b07e26ebfd636149ded0274d7be624e9b521af475218d7654e6f190000000400" ) )
      .unwrap<Expression>()
      .unwrap<Object>()
      .unwrap<Value>()
      .unwrap<Blob>() );
  // auto linkelfs = Handle<BlobRef>( Handle<Fix>::forge( base16::decode(
  //"8276ca7ec55e2be248a81fc23be64d6fc78ff03e902351326127150300000400" )
  //).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>() );

  auto application = OwnedMutTree::allocate( 3 );
  application[0] = make_limits( rt->get_rt(), 1024 * 1024 * 1024, 1024 * 1024, 1 );
  application[1] = compile_encode;
  // application[2] = linkelfs;
  application[2] = wasm2c;

  auto handle = rt->get_rt().create( make_shared<OwnedTree>( std::move( application ) ) ).unwrap<ValueTree>();

  auto res = rt->execute( Handle<Eval>( Handle<Object>( Handle<Application>( handle::upcast( handle ) ) ) ) );

  // print the result
  cout << "Result:\n" << res << endl;

  return 0;
}