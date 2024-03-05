#include "handle.hh"
#include "relater.hh"
#include "test.hh"
#include <memory>

using namespace std;

auto rt = make_shared<Relater>();

void test( void )
{
  auto thunk = Handle<Application>(
    handle::upcast( tree( *rt,
                          limits( *rt, 16, 0, 1 ),
                          compile( *rt, ( file( *rt, "testing/wasm-examples/addblob.wasm" ) ) ),
                          1_literal64,
                          2_literal64 ) ) );
  rt->execute( Handle<Eval>( thunk ) );
}
