#include <iostream>
#include <unistd.h>

#include "handle.hh"
#include "runtimes.hh"
#include "tester-utils.hh"
#include "types.hh"

using namespace std;

auto rt = make_shared<Relater>();

int main()
{
  auto compile_encode = rt->labeled( "compile-encode" ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<ValueTree>();
  rt->get( compile_encode );

  auto wasm2c = Handle<BlobRef>( rt->labeled( "wasm-to-c-fix-wasm" ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>() );

  auto application = OwnedMutTree::allocate( 3 );
  application[0] = make_limits( *rt, 1024 * 1024 * 1024, 1024 * 1024, 1); 
  application[1] = compile_encode;
  application[2] = wasm2c;

  auto handle = rt->create( make_shared<OwnedTree>( std::move( application ) ) ).unwrap<ValueTree>();
  auto res = rt->execute( Handle<Eval>( Handle<Object>( Handle<Application>( handle::upcast( handle ) ) ) ) );

  // print the result
  cout << "Result:\n" << res << endl;

  return 0;
}
