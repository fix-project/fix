#include "bptree-helper.hh"
#include "option-parser.hh"
#include "relater.hh"
#include <fstream>
#include <memory>
#include <random>

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc < 7 ) {
    cerr << "Usage: " << argv[0]
         << " [degree] [number of entries] [path to list of keys] [number of samples] [path to randomly sampled "
            "keys in tree] [path to randomly sampled keys not in tree]"
         << endl;
  }

  auto rt = std::make_shared<Relater>( 0 );
  size_t degree = stoi( argv[1] );
  BPTree tree( degree );
  size_t num_of_entries = stoi( argv[2] );

  ofstream keys_file( argv[3] );

  random_device rd;
  mt19937 gen( rd() );
  uniform_int_distribution<int> distrib( INT_MIN, INT_MAX );
  set<int> key_set {};

  while ( key_set.size() < num_of_entries ) {
    auto idx = distrib( gen );
    if ( !key_set.contains( idx ) ) {
      tree.insert( idx, std::to_string( idx ) );
      key_set.insert( idx );
    }
  }

  for ( const auto& key : key_set ) {
    keys_file << key << endl;
  }

  keys_file.close();

  size_t num_of_samples = stoi( argv[4] );

  {
    unordered_set<int> keys_in_tree;
    ofstream keys_in_tree_sample( argv[5] );
    uniform_int_distribution<int> distrib( 0, key_set.size() );
    while ( keys_in_tree.size() < num_of_samples ) {
      auto idx = distrib( gen );
      auto key = *next( key_set.begin(), idx );
      if ( !keys_in_tree.contains( key ) ) {
        keys_in_tree.insert( key );
        keys_in_tree_sample << key << endl;
      }
    }
  }

  {
    unordered_set<int> keys_not_in_tree;
    ofstream keys_not_in_tree_sample( argv[6] );
    uniform_int_distribution<int> distrib( INT_MIN, INT_MAX );
    while ( keys_not_in_tree.size() < num_of_samples ) {
      auto key = distrib( gen );
      if ( !key_set.contains( key ) and !keys_not_in_tree.contains( key ) ) {
        keys_not_in_tree.insert( key );
        keys_not_in_tree_sample << key << endl;
      }
    }
  }

  cout << Handle<Fix>( bptree::to_repo( rt->get_storage(), rt->get_repository(), tree ) ).content << endl;
}
