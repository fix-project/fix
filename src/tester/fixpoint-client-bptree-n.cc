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

  if ( argc != 5 ) {
    cerr << "Usage: +[address]:[port] [key-list] [# of keys] [# of leaf nodes to get]" << endl;
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
  vector<int> keys;
  for ( int i = 0; i < atoi( argv[3] ); i++ ) {
    int key;
    keys_list >> key;
    keys.push_back( key );
  }

  auto parallel_tree = OwnedMutTree::allocate( keys.size() );
  size_t index = 0;
  uint64_t n = atoi( argv[4] );
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
