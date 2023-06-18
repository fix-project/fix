#include <iostream>
#include <string>

#include "handle.hh"
#include "mmap.hh"
#include "runtimestorage.hh"
#include "timer.hh"

#define INIT_INSTANCE 4096

using namespace std;

struct addend
{
  uint32_t a;
  uint32_t b;
};

addend arguments[INIT_INSTANCE];
auto& runtime = RuntimeStorage::get_instance();
Handle add_fixpoint_thunk[INIT_INSTANCE];

addend make_addend( int i )
{
  addend result;
  result.a = i;
  result.b = 2 * i;
  return result;
}

int main( int argc, char* argv[] )
{
  if ( argc < 2 ) {
    cerr << "Usage: " << argv[0] << " path_to_add_wasm_file\n";
  }

  ReadOnlyFile wasm_content { argv[1] };

  Handle add_name = runtime.add_blob( string_view( wasm_content ) );
  runtime.populate_program( add_name );

  // Populate addends
  for ( int i = 0; i < INIT_INSTANCE; i++ ) {
    arguments[i] = make_addend( i );
  }

  // Populate encodes
  for ( int i = 0; i < INIT_INSTANCE; i++ ) {
    Handle arg1 = runtime.add_blob( make_blob( arguments[i].a ) );
    Handle arg2 = runtime.add_blob( make_blob( arguments[i].b ) );

    Tree_ptr fix_encode { static_cast<Handle*>( aligned_alloc( alignof( Handle ), sizeof( Handle ) * 4 ) ) };
    if ( not fix_encode ) {
      throw bad_alloc();
    }

    fix_encode.get()[0] = Handle( "empty" );
    fix_encode.get()[1] = add_name;
    fix_encode.get()[2] = arg1;
    fix_encode.get()[3] = arg2;

    Handle fix_encode_name = runtime.add_tree( { std::move( fix_encode ), 4 } );

    Thunk fix_thunk( fix_encode_name );
    Handle fix_thunk_name = runtime.add_thunk( fix_thunk );
    add_fixpoint_thunk[i] = fix_thunk_name;
  }

  {
    GlobalScopeTimer<Timer::Category::Execution> record_timer;
    for ( int i = 0; i < INIT_INSTANCE; i++ ) {
      runtime.eval_thunk( add_fixpoint_thunk[i] );
    }
  }
  global_timer().average( cout, INIT_INSTANCE );
  reset_global_timer();

  return 0;
}
