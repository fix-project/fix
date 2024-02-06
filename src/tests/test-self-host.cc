#include <string>

#include "test.hh"

using namespace std;

auto rt = ReadOnlyRT::init();

void test( void )
{
  array<std::string, 5> tasks = { "wasm-to-c-fix", "c-to-elf-fix", "link-elfs-fix", "compile", "map" };
  vector<Handle<Fix>> wasm_names;
  for ( auto task : tasks ) {
    wasm_names.push_back( rt->labeled( task + "-wasm" ) );
  }

  // Apply compile-encode to each wasm
  auto new_elf_thunks = OwnedMutTree::allocate( wasm_names.size() );
  for ( size_t i = 0; i < wasm_names.size(); i++ ) {
    new_elf_thunks[i] = compile( *rt, wasm_names[i] ).unwrap<Thunk>();
  }

  auto new_elf_thunk_tree = rt->create( make_shared<OwnedTree>( std::move( new_elf_thunks ) ) );
  auto new_elfs = rt->execute( Handle<Eval>( new_elf_thunk_tree.try_into<ObjectTree>().value() ) );
  auto new_elf_names = rt->get( new_elfs.try_into<ValueTree>().value() ).value();

  size_t index = 0;
  for ( auto task : tasks ) {
    Handle runnable_tag = rt->labeled( task + "-runnable-tag" );
    Handle new_runnable_tag = new_elf_names->at( index );
    if ( runnable_tag != new_runnable_tag ) {
      fprintf( stderr, "%s: Failed to get runnable tag\n", task.c_str() );
      exit( 1 );
    }
    index++;
  }
}
