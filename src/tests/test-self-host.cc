#include <string>

#include "test.hh"

using namespace std;

void test( void )
{
  auto& runtime = Runtime::get_instance();

  runtime.storage().deserialize();

  array<std::string, 5> tasks = { "wasm-to-c-fix", "c-to-elf-fix", "link-elfs-fix", "compile", "map" };
  vector<Handle> wasm_names;
  for ( auto task : tasks ) {
    wasm_names.push_back( *runtime.storage().get_ref( task + "-wasm" ) );
  }

  // Apply compile-encode to each wasm
  auto new_elf_thunks = OwnedMutTree::allocate( wasm_names.size() );
  for ( size_t i = 0; i < wasm_names.size(); i++ ) {
    new_elf_thunks[i] = compile( wasm_names[i] );
  }

  Handle new_elf_thunk_tree = runtime.storage().add_tree( std::move( new_elf_thunks ) );
  Handle new_elfs = runtime.eval( new_elf_thunk_tree );
  auto new_elf_names = runtime.storage().get_tree( new_elfs );

  size_t index = 0;
  for ( auto task : tasks ) {
    Handle runnable_tag = *runtime.storage().get_ref( task + "-runnable-tag" );
    Handle new_runnable_tag = new_elf_names[index];
    if ( not runtime.storage().compare_handles( runnable_tag, new_runnable_tag ) ) {
      fprintf( stderr, "%s: Failed to get runnable tag\n", task.c_str() );
      exit( 1 );
    }
    index++;
  }
}
