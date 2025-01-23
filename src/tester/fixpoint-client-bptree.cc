#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>

#include "object.hh"
#include "runtimes.hh"
#include "tester-utils.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  shared_ptr<Client> rt;

  if ( argc != 7 ) {
    cerr << "Usage: " << argv[0]
         << " +[address]:[port] [key-list] [begin key index] [# of keys] [bptree-get-label] [tree root label]"
         << endl;
    return -1;
  }

  if ( argv[1][0] == '+' ) {
    string addr( &argv[1][1] );
    if ( addr.find( ':' ) == string::npos ) {
      throw runtime_error( "invalid argument " + addr );
    }
    Address address( addr.substr( 0, addr.find( ':' ) ), stoi( addr.substr( addr.find( ':' ) + 1 ) ) );
    rt = Client::init( address );
  } else {
    exit( 1 );
  }

  ifstream keys_list( argv[2] );
  int begin_index = atoi( argv[3] );
  int num_keys = atoi( argv[4] );

  vector<int> keys;
  for ( int i = 0; i < begin_index + num_keys; i++ ) {
    int key;
    keys_list >> key;
    if ( i >= begin_index ) {
      keys.push_back( key );
    }
  }

  auto bptree_get
    = rt->get_rt().labeled( argv[5] ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<ValueTree>();

  auto tree_root
    = rt->get_rt().labeled( argv[6] ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<ValueTreeRef>();

  Handle<Encode> select;
  if ( string( argv[5] ) == "bptree-get-good" ) {
    auto selection_tree = OwnedMutTree::allocate( 2 );
    selection_tree[0] = tree_root;
    selection_tree[1] = Handle<Literal>( (uint64_t)0 );
    select = Handle<Strict>( Handle<Selection>( Handle<ObjectTree>(
      rt->get_rt().create( make_shared<OwnedTree>( std::move( selection_tree ) ) ).unwrap<ValueTree>() ) ) );
  } else {
    auto selection_tree = OwnedMutTree::allocate( 3 );
    selection_tree[0] = tree_root;
    selection_tree[1] = Handle<Literal>( (uint64_t)0 );
    selection_tree[2] = Handle<Literal>( (uint64_t)( 16777218 + 1 ) );
    select = Handle<Shallow>( Handle<Selection>( Handle<ObjectTree>(
      rt->get_rt().create( make_shared<OwnedTree>( std::move( selection_tree ) ) ).unwrap<ValueTree>() ) ) );
  }

  auto parallel_tree = OwnedMutTree::allocate( keys.size() );
  size_t index = 0;
  for ( auto key : keys ) {
    auto tree = OwnedMutTree::allocate( 5 );
    tree.at( 0 ) = make_limits( rt->get_rt(), 1024 * 1024 * 1024, 1024, 1 ).into<Fix>();
    tree.at( 1 ) = bptree_get;
    tree.at( 2 ) = select;
    tree.at( 3 ) = tree_root;
    tree.at( 4 ) = Handle<Literal>( (int)key );
    auto combination = rt->get_rt().create( make_shared<OwnedTree>( std::move( tree ) ) ).unwrap<ExpressionTree>();
    auto application = Handle<Application>( combination );
    parallel_tree[index] = application;
    index++;
  }

  auto combination
    = rt->get_rt().create( make_shared<OwnedTree>( std::move( parallel_tree ) ) ).unwrap<ObjectTree>();

  auto start = chrono::steady_clock::now();
  auto res = rt->execute( Handle<Eval>( combination ) );
  auto end = chrono::steady_clock::now();
  chrono::duration<double> diff = end - start;

  cout << "Result:\n" << res << endl;
  cout << "Duration [seconds]: " << diff << endl;

  return 0;
}
