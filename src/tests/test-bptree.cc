#include "bptree.hh"

#include <random>
#include <string>
#include <unordered_set>

void test( void )
{
  size_t degree = 4;
  BPTree<int, std::string> tree( degree );

  std::random_device rd;
  std::mt19937 gen( rd() );
  std::uniform_int_distribution<int> distrib( 1, 256 );
  std::unordered_set<int> key_set {};

  for ( size_t i = 0; i < 256; i++ ) {
    auto idx = distrib( gen );
    tree.insert( idx, std::to_string( idx ) );

    auto res = tree.get( idx );
    if ( !res.has_value() or res.value() != std::to_string( idx ) ) {
      fprintf( stderr, "Insertion failed." );
      exit( 1 );
    }

    key_set.insert( idx );
  }

  int last = 0;

  tree.dfs_visit( [&]( Node<int, std::string>* node ) {
    if ( node->is_leaf() ) {
      for ( const auto& key : node->get_keys() ) {
        if ( key <= last ) {
          fprintf( stderr, "BPTree: leaves not sorted." );
          exit( 1 );
        }

        last = key;
      }
    }
  } );

  tree.dfs_visit( [&]( Node<int, std::string>* node ) {
    if ( node->is_leaf() ) {
      if ( node->get_keys().size() > degree ) {
        fprintf( stderr, "Leaf node violates degree" );
        exit( 1 );
      }
    } else {
      if ( node->get_keys().size() + 1 > degree or node->get_keys().size() + 1 < degree / 2 ) {
        fprintf( stderr, "Internal node violates degree" );
        exit( 1 );
      }
    }
  } );

  for ( const auto& key : key_set ) {
    auto res = tree.get( key );
    if ( !res.has_value() or res.value() != std::to_string( key ) ) {
      fprintf( stderr, "Tree corrupted." );
      exit( 1 );
    }
  }
}
