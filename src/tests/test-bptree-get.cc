#include "bptree-helper.hh"
#include "handle.hh"
#include "relater.hh"
#include "test.hh"
#include <cstdio>
#include <random>

using namespace std;

string fix_bptree_get( shared_ptr<Relater> rt, Handle<Value> bptree_fix, Handle<Fix> bptree_elf, int key )
{
  auto combination = tree( *rt,
                           limits( *rt, 1024 * 1024, 1024, 1 ).into<Fix>(),
                           bptree_elf,
                           Handle<Strict>( Handle<Selection>( Handle<ObjectTree>(
                             tree( *rt, bptree_fix, Handle<Literal>( (uint64_t)0 ) ).unwrap<ValueTree>() ) ) ),
                           bptree_fix,
                           Handle<Literal>( key ) )
                       .visit<Handle<ExpressionTree>>( []( auto h ) { return Handle<ExpressionTree>( h ); } );
  auto bptree_get = Handle<Application>( combination );
  auto result = rt->execute( Handle<Eval>( bptree_get ) );

  optional<string> res = result.try_into<Blob>().transform( [&]( auto h ) {
    return h.template visit<string>( overload {
      []( Handle<Literal> l ) -> string { return string( l.view() ); },
      [&]( Handle<Named> b ) -> string {
        auto span = rt->get( b ).value()->span();
        return string( span.data(), span.size() );
      },
    } );
  } );

  if ( res ) {
    return res.value();
  } else {
    throw runtime_error( "Invalid bptree-get result." );
  }
}

string fix_bptree_get_n( shared_ptr<Relater> rt,
                         Handle<Value> bptree_fix,
                         Handle<Fix> bptree_get_n_elf,
                         int key,
                         uint64_t n )
{
  auto combination = tree( *rt,
                           limits( *rt, 1024 * 1024, 1024, 1 ).into<Fix>(),
                           bptree_get_n_elf,
                           Handle<Strict>( Handle<Selection>( Handle<ObjectTree>(
                             tree( *rt, bptree_fix, Handle<Literal>( (uint64_t)0 ) ).unwrap<ValueTree>() ) ) ),
                           bptree_fix,
                           Handle<Literal>( key ),
                           Handle<Literal>( n ) )
                       .visit<Handle<ExpressionTree>>( []( auto h ) { return Handle<ExpressionTree>( h ); } );
  auto bptree_get_n = Handle<Application>( combination );
  auto result = rt->execute( Handle<Eval>( bptree_get_n ) );

  string res;
  rt->visit_minrepo( result, [&]( Handle<AnyDataType> x ) {
    x.visit<void>( overload { [&]( Handle<Literal> l ) {
                               res += l.data();
                               res += " ";
                             },
                              [&]( Handle<Named> n ) {
                                res += string_view( rt->get( n ).value()->span() );
                                res += " ";
                              },
                              []( auto ) {} } );
  } );

  return res;
}

void check_bptree_get( shared_ptr<Relater> rt,
                       Handle<Value> bptree_fix,
                       Handle<Fix> bptree_elf,
                       BPTree<int, string>& tree,
                       int key )
{
  string fix_result = fix_bptree_get( rt, bptree_fix, bptree_elf, key );
  auto result = tree.get( key );

  if ( ( result.has_value() && result.value() != fix_result ) || ( !result.has_value() && fix_result != "" ) ) {
    fprintf( stderr,
             "bptree_get: got %s, expected %s.\n",
             fix_result.c_str(),
             ( result.has_value() ? result.value().c_str() : "" ) );
    exit( 1 );
  }
}

void check_bptree_get_n( shared_ptr<Relater> rt,
                         Handle<Value> bptree_fix,
                         Handle<Fix> bptree_get_n_elf,
                         BPTree<int, string>& tree,
                         int key,
                         uint64_t n )
{
  string fix_result = fix_bptree_get_n( rt, bptree_fix, bptree_get_n_elf, key, n );

  string result;
  optional<size_t> n_done;

  tree.dfs_visit( [&]( Node<int, string>* node ) {
    if ( node->is_leaf() ) {
      if ( n_done.has_value() && *n_done < n ) {
        for ( const auto& d : node->get_data() ) {
          result += d;
          result += " ";
          n_done.value()++;

          if ( n_done.value() == n ) {
            return;
          }
        }

        return;
      }

      if ( node->get_keys().front() <= key && node->get_keys().back() >= key ) {
        n_done = 0;
        auto pos = upper_bound( node->get_keys().begin(), node->get_keys().end(), key );
        auto idx = pos - node->get_keys().begin() - 1;
        for ( size_t i = idx; i < node->get_data().size(); i++ ) {
          result += node->get_data()[i];
          result += " ";
          n_done.value()++;

          if ( n_done.value() == n ) {
            return;
          }
        }
      }
    }
  } );

  if ( fix_result != result ) {
    fprintf( stderr, "bptree_get_n: got %s, expected %s.\n", fix_result.c_str(), result.c_str() );
    exit( 1 );
  }
}

void test( shared_ptr<Relater> rt )
{
  size_t degree = 4;
  BPTree<int, string> tree( degree );

  random_device rd;
  mt19937 gen( rd() );
  uniform_int_distribution<int> distrib( 1, 256 );
  set<int> key_set {};

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

  auto bptree_fix = bptree::to_storage( rt->get_storage(), tree );

  Handle<Fix> bptree_elf
    = compile( *rt, file( *rt, "applications-prefix/src/applications-build/bptree-get/bptree-get.wasm" ) );
  Handle<Fix> bptree_get_n_elf
    = compile( *rt, file( *rt, "applications-prefix/src/applications-build/bptree-get/bptree-get-n.wasm" ) );

  for ( const auto& key : key_set ) {
    check_bptree_get( rt, bptree_fix, bptree_elf, tree, key );
    auto res = tree.get( key );
    if ( !res.has_value() or res.value() != std::to_string( key ) ) {
      fprintf( stderr, "Tree corrupted." );
      exit( 1 );
    }
  }

  check_bptree_get_n( rt, bptree_fix, bptree_get_n_elf, tree, *key_set.begin(), 10 );
  check_bptree_get_n( rt, bptree_fix, bptree_get_n_elf, tree, *std::next( key_set.begin(), 100 ), 10 );
  check_bptree_get_n( rt, bptree_fix, bptree_get_n_elf, tree, *std::next( key_set.end(), -4 ), 10 );
}
