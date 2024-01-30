#include "handle.hh"
#include "test.hh"

using namespace std;

auto rt = ReadOnlyTester::init();

void test( void )
{
  auto thunk = Handle<Application>( handle::upcast(
    tree( *rt, blob( *rt, "unused" ), compile( *rt, ( file( *rt, "testing/wasm-examples/trap.wasm" ) ) ) ) ) );
  rt->executor().execute( Handle<Eval>( thunk ) );
}
