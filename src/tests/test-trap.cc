#include "handle.hh"
#include "relater.hh"
#include "test.hh"
#include <memory>

using namespace std;

auto rt = make_shared<Relater>();

void test( void )
{
  auto thunk = Handle<Application>( handle::upcast(
    tree( *rt, blob( *rt, "unused" ), compile( *rt, ( file( *rt, "testing/wasm-examples/trap.wasm" ) ) ) ) ) );
  rt->execute( Handle<Eval>( thunk ) );
}
