#include "bptree-helper.hh"
#include "option-parser.hh"
#include "relater.hh"
#include <memory>
#include <random>

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc < 3 ) {
    cerr << "Usage: " << argv[0] << " [degree] [number-of-entries]" << endl;
  }

  auto rt = std::make_shared<Relater>( 0 );
  size_t degree = stoi( argv[1] );
  BPTree tree( degree );
  size_t num_of_entries = stoi( argv[2] );

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

  cout << Handle<Fix>( bptree::to_repo( rt->get_storage(), rt->get_repository(), tree ) ).content << endl;
}
