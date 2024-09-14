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

  if ( argc != 6 ) {
    cerr << "Usage: +[address]:[port] [key-list] [begin key index] [# of keys] [# of leaf nodes to get]" << endl;
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

  auto bptree_get_n = rt->get_rt()
                      .labeled( "bptree-get-n" )
                      .unwrap<Expression>()
                      .unwrap<Object>()
                      .unwrap<Value>()
                      .unwrap<ValueTree>();
  rt->get_rt().get( bptree_get_n );

  auto tree_root = rt->get_rt()
                     .labeled( "tree-root" )
                     .unwrap<Expression>()
                     .unwrap<Object>()
                     .unwrap<Value>()
                     .unwrap<ValueTreeRef>();

  auto selection_tree = OwnedMutTree::allocate( 2 );
  selection_tree[0] = tree_root;
  selection_tree[1] = Handle<Literal>( (uint64_t)0 );
  auto select = rt->get_rt().create( make_shared<OwnedTree>( std::move( selection_tree ) ) ).unwrap<ValueTree>();

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

  uint64_t n = atoi( argv[5] );

  auto start = chrono::steady_clock::now();

  for ( auto key : keys ) {
    auto tree = OwnedMutTree::allocate( 6 );
    tree.at( 0 ) = make_limits( rt->get_rt(), 1024 * 1024, 1024, 1 ).into<Fix>();
    tree.at( 1 ) = bptree_get_n;
    tree.at( 2 ) = Handle<Strict>( Handle<Selection>( Handle<ObjectTree>( select ) ) );
    tree.at( 3 ) = tree_root;
    tree.at( 4 ) = Handle<Literal>( (int)key );
    tree.at( 5 ) = Handle<Literal>( (uint64_t)n );
    auto combination = rt->get_rt().create( make_shared<OwnedTree>( std::move( tree ) ) ).unwrap<ExpressionTree>();
    auto application = Handle<Application>( combination );
    rt->execute( Handle<Eval>( application ) );
  }

  auto end = chrono::steady_clock::now();
  chrono::duration<double> diff = end - start;

  cout << "Duration [seconds]: " << diff << endl;

  return 0;
}
