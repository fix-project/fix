#pragma once

#include "readonlyrt.hh"

#pragma GCC diagnostic ignored "-Wunused-function"

static Handle<Strict> compile( ReadOnlyTester& rt, Handle<Fix> wasm )
{
  auto compiler = rt.labeled( "compile-encode" );

  auto tree = OwnedMutTree::allocate( 3 );
  tree.at( 0 ) = Handle<Literal>( "unused" );
  tree.at( 1 ) = compiler;
  tree.at( 2 ) = wasm;
  return rt.create( std::make_shared<OwnedTree>( std::move( tree ) ) ).visit<Handle<Strict>>( []( auto x ) {
    return Handle<Strict>( Handle<Application>( Handle<ExpressionTree>( x ) ) );
  } );
}

template<FixHandle... Args>
Handle<AnyTree> tree( ReadOnlyTester& rt, Args... args )
{
  OwnedMutTree tree = OwnedMutTree::allocate( sizeof...( args ) );
  size_t i = 0;
  (void)i;
  (
    [&] {
      tree[i] = args;
      i++;
    }(),
    ... );

  return rt.create( std::make_shared<OwnedTree>( std::move( tree ) ) );
}

static Handle<Blob> blob( ReadOnlyTester& rt, std::string_view contents )
{
  auto blob = OwnedMutBlob::allocate( contents.size() );
  memcpy( blob.data(), contents.data(), contents.size() );
  return rt.create( std::make_shared<OwnedBlob>( std::move( blob ) ) );
}

static Handle<Blob> file( ReadOnlyTester& rt, std::filesystem::path path )
{
  return rt.create( std::make_shared<OwnedBlob>( path ) );
}
