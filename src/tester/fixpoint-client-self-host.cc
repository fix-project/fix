#include <iostream>
#include <unistd.h>

#include "handle_post.hh"
#include "runtimes.hh"
#include "tester-utils.hh"

using namespace std;

int min_args = 0;
int max_args = -1;

int main( int argc, char* argv[] )
{
  std::shared_ptr<Client> client;

  if ( argc != 2 ) {
    cerr << "Usage: +[address]:[port]" << endl;
    exit( 1 );
  }

  if ( argv[1][0] == '+' ) {
    string addr( &argv[1][1] );
    if ( addr.find( ':' ) == string::npos ) {
      throw runtime_error( "invalid argument " + addr );
    }
    Address address( addr.substr( 0, addr.find( ':' ) ), stoi( addr.substr( addr.find( ':' ) + 1 ) ) );
    client = Client::init( address );
  } else {
    exit( 1 );
  }

  array<std::string, 5> tasks = { "wasm-to-c-fix", "c-to-elf-fix", "link-elfs-fix", "compile", "map" };
  vector<Handle<Thunk>> wasm_names;
  for ( auto task : tasks ) {
    auto id = handle::extract<Identification>( make_identification( client->get_rt().labeled( task + "-wasm" ) ) )
                .value();
    wasm_names.push_back( id );
  }

  // Apply compile-encode to each wasm
  auto new_elf_thunks = OwnedMutTree::allocate( wasm_names.size() );
  for ( size_t i = 0; i < wasm_names.size(); i++ ) {
    new_elf_thunks[i] = make_compile( client->get_rt(), Handle<Strict>( wasm_names[i] ) );
  }

  auto handle
    = client->get_rt().create( make_shared<OwnedTree>( std::move( new_elf_thunks ) ) ).unwrap<ObjectTree>();

  auto res = client->execute( Handle<Eval>( Handle<Object>( handle ) ) );

  // print the result
  cout << "Result:\n" << res << endl;

  return 0;
}
