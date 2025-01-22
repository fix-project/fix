#include <iostream>
#include <memory>
#include <unistd.h>

#include "handle.hh"
#include "object.hh"
#include "runtimes.hh"
#include "tester-utils.hh"
#include "types.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  std::shared_ptr<Client> client;

  if ( argc != 4 and argc != 5 ) {
    cerr << "Usage: " << argv[0] << " +[address]:[port] input-wasm2c-output c-to-elf-runnable [link-elfs-runnable]"
         << endl;
    abort();
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

  auto& rt = client->get_rt();

  auto input = rt.labeled( argv[2] ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<ValueTree>();
  auto input_tree = rt.get( input ).value();
  auto impl_h = input_tree->at( 0 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();
  auto input_c_file_tree
    = rt.get( input_tree->at( 1 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<ValueTree>() )
        .value();

  auto c_to_elf_runnable = rt.labeled( argv[3] ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();
  auto system_dep = rt.labeled( "system-dep-tree" ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();
  auto clang_dep = rt.labeled( "clang-dep-tree" ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();

  optional<Handle<Value>> link_elfs_runnable;
  if ( argc == 5 ) {
    link_elfs_runnable = rt.labeled( argv[4] ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();
  }

  auto cc_args = OwnedMutTree::allocate( 4 );
  cc_args[0] = make_limits( rt, 1024 * 1024 * 1024, 1024 * 1024, 1 );
  cc_args[1] = c_to_elf_runnable;
  cc_args[2] = Handle<Strict>( Handle<Identification>( system_dep ) );
  cc_args[3] = Handle<Strict>( Handle<Identification>( clang_dep ) );
  auto curried_cc = rt.create( make_shared<OwnedTree>( move( cc_args ) ) ).unwrap<ExpressionTree>();

  auto arg_num = input_c_file_tree->size();
  auto args = OwnedMutTree::allocate( arg_num );
  for ( size_t i = 0; i < arg_num; i++ ) {
    auto application = OwnedMutTree::allocate( 4 );
    application[0] = make_limits( rt, 1024 * 1024 * 1024, 1024 * 1024, 1 );
    application[1] = curried_cc;
    application[2] = impl_h;
    application[3] = input_c_file_tree->at( i );
    auto handle = rt.create( make_shared<OwnedTree>( move( application ) ) ).unwrap<ExpressionTree>();
    auto apply = Handle<Application>( handle );

    if ( link_elfs_runnable.has_value() ) {
      args[i] = Handle<Strict>( apply );
    } else {
      args[i] = apply;
    }
  }

  auto arg_tree = rt.create( make_shared<OwnedTree>( move( args ) ) );

  Handle<Relation> top_level;
  if ( link_elfs_runnable.has_value() ) {
    auto application = OwnedMutTree::allocate( 3 );
    application[0] = make_limits( rt, 1024 * 1024 * 1024, 1024 * 1024, 1 );
    application[1] = link_elfs_runnable.value();
    application[2] = arg_tree.unwrap<ExpressionTree>();
    auto handle = rt.create( make_shared<OwnedTree>( move( application ) ) ).unwrap<ExpressionTree>();
    auto apply = Handle<Application>( handle );

    top_level = Handle<Eval>( apply );
  } else {
    top_level = Handle<Eval>( arg_tree.unwrap<ObjectTree>() );
  }

  auto start = chrono::steady_clock::now();
  auto res = client->execute( top_level );
  auto end = chrono::steady_clock::now();
  chrono::duration<double> diff = end - start;

  // print the result
  cout << "Result:\n" << res << endl;
  cout << "Duration [seconds]: " << diff << endl;

  return 0;
}
