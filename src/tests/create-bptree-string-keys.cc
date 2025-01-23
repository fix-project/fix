#include "bptree-helper.hh"
#include "option-parser.hh"
#include "relater.hh"
#include <fstream>
#include <memory>
#include <string>

using namespace std;

template<>
string_view Node<string, int>::get_key_data()
{
  for ( const auto& k : keys_ ) {
    size_t key_size = k.size();
    string buf;
    buf.resize( sizeof( size_t ) + k.size() );
    memcpy( buf.data(), &key_size, sizeof( size_t ) );
    memcpy( buf.data() + sizeof( size_t ), k.data(), k.size() );
    key_data_buf_ += buf;
  }

  return { key_data_buf_ };
}

int main( int argc, char* argv[] )
{
  if ( argc < 3 ) {
    cerr << "Usage: " << argv[0] << " degree path-to-list-of-keys" << endl;
    abort();
  }

  auto rt = std::make_shared<Relater>( 0 );
  size_t degree = stoi( argv[1] );
  BPTree<string, int> tree( degree );
  ifstream keys_file( argv[2] );

  if ( degree < 100000 ) {
    string line;
    int counter = 0;
    while ( getline( keys_file, line ) ) {
      tree.insert( line, counter );
      counter++;
      if ( counter % 10000 == 0 ) {
        cout << std::chrono::system_clock::now() << " " << counter << endl;
      }
    }
  } else {
    string line;
    int counter = 0;
    map<string, int> map;
    while ( getline( keys_file, line ) ) {
      map.insert( { line, counter } );
      counter++;
      if ( counter % 10000 == 0 ) {
        cout << std::chrono::system_clock::now() << " " << counter << endl;
      }
    }

    vector<string> keys;
    vector<int> data;

    for ( auto& [k, d] : map ) {
      keys.push_back( k );
      data.push_back( d );
    }

    auto root = make_shared<Node<string, int>>();
    root->set_keys( keys );
    root->set_data( data );

    tree.set_root( root );
  }

  cout << Handle<Fix>( bptree::to_repo( rt->get_storage(), rt->get_repository(), tree ) ).content << endl;
}
