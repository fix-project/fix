#pragma once
#include "mmap.hh"
#include "runtime.hh"

#pragma GCC diagnostic ignored "-Wunused-function"

static std::vector<ReadOnlyFile> files {};

static Handle thunk( Handle tree )
{
  auto& rt = Runtime::get_instance();
  return rt.storage().add_thunk( Thunk( tree ) );
};

static Handle compile( Handle wasm )
{
  auto& rt = Runtime::get_instance();
  auto compiler = rt.storage().get_ref( "compile-encode" );
  if ( not compiler.has_value() ) {
    fprintf( stderr, "Ref 'compile-encode' not defined." );
    exit( 1 );
  }

  Tree tree { 3 };
  tree.at( 0 ) = Handle( "unused" );
  tree.at( 1 ) = *compiler;
  tree.at( 2 ) = wasm;
  return thunk( rt.storage().add_tree( std::move( tree ) ) );
}

static Handle tree( std::initializer_list<Handle> elements )
{
  auto& rt = Runtime::get_instance();
  Tree t { static_cast<uint32_t>( elements.size() ) };
  size_t i = 0;
  for ( Handle element : elements ) {
    t.at( i++ ) = element;
  }
  return rt.storage().add_tree( std::move( t ) );
}

static Handle blob( std::string_view contents )
{
  auto& rt = Runtime::get_instance();
  return rt.storage().add_blob( contents );
}

static Handle file( std::filesystem::path path )
{
  auto& rt = Runtime::get_instance();
  files.emplace_back( path );
  return rt.storage().add_blob( std::string_view( files.back() ) );
}
