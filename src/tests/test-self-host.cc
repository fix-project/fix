#include <fstream>
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
  vector<Handle> new_elf_thunks;
  for ( auto wasm : wasm_names ) {
    new_elf_thunks.push_back( compile( wasm ) );
  }

  Handle new_elf_thunk_tree
    = runtime.storage().add_tree( span_view<Handle>( new_elf_thunks.data(), new_elf_thunks.size() ) );
  Handle new_elfs = runtime.eval( new_elf_thunk_tree );
  auto new_elf_names = runtime.storage().get_tree( new_elfs );

  size_t index = 0;
  for ( auto task : tasks ) {
    Handle runnable_tag = *runtime.storage().get_ref( task + "-runnable-tag" );
    Handle new_runnable_tag = new_elf_names.at( index );
    if ( not runtime.storage().compare_handles( runnable_tag, new_runnable_tag ) ) {
      fprintf( stderr, "%s: Failed to get runnable tag\n", task.c_str() );
      exit( 1 );
    }
    index++;
  }
}
