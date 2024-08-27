#include "bptree-helper.hh"
#include "option-parser.hh"
#include "relater.hh"
#include <memory>
#include <random>
#include <fstream>

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc < 4 ) {
    cerr << "Usage: " << argv[0] << " [degree] [number-of-entries] [path-to-list-of-keys]" << endl;
  }

  auto rt = std::make_shared<Relater>( 0 );
  size_t degree = stoi( argv[1] );
  BPTree tree( degree );
  size_t num_of_entries = stoi( argv[2] );
  ofstream keys_file;
  keys_file.open( argv[3] );

  random_device rd;
  mt19937 gen( rd() );
  uniform_int_distribution<int> distrib( INT_MIN, INT_MAX );
  unordered_set<int> key_set {};

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

  cout << Handle<Fix>( bptree::to_repo( rt->get_storage(), rt->get_repository(), tree ) ).content << endl;
}
