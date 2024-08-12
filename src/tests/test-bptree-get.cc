#include "bptree-helper.hh"
#include "relater.hh"
#include "test.hh"
#include <cstdio>
#include <random>

using namespace std;

string fix_bptree_get( shared_ptr<Relater> rt, Handle<Value> bptree_fix, Handle<Fix> bptree_elf, uint64_t key )
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

void check_bptree_get( shared_ptr<Relater> rt,
                       Handle<Value> bptree_fix,
                       Handle<Fix> bptree_elf,
                       BPTree& tree,
                       uint64_t key )
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

void test( shared_ptr<Relater> rt )
{
  size_t degree = 4;
  BPTree tree( degree );

  random_device rd;
  mt19937 gen( rd() );
  uniform_int_distribution<size_t> distrib( 1, 256 );
  unordered_set<size_t> key_set {};

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

  auto bptree_fix = bptree::to_rt( *rt, tree );

  Handle<Fix> bptree_elf
    = compile( *rt, file( *rt, "applications-prefix/src/applications-build/bptree-get/bptree-get.wasm" ) );

  for ( const auto& key : key_set ) {
    check_bptree_get( rt, bptree_fix, bptree_elf, tree, key );
    auto res = tree.get( key );
    if ( !res.has_value() or res.value() != std::to_string( key ) ) {
      fprintf( stderr, "Tree corrupted." );
      exit( 1 );
    }
  }
}
