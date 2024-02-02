#pragma once
#include "mmap.hh"
#include "runtime.hh"

#pragma GCC diagnostic ignored "-Wunused-function"

static Handle thunk( Handle tree )
{
  return tree.as_thunk();
};

static Handle compile( Handle wasm )
{
  auto& rt = Runtime::get_instance();
  auto compiler = rt.storage().get_ref( "compile-encode" );
  if ( not compiler.has_value() ) {
    fprintf( stderr, "Ref 'compile-encode' not defined." );
    exit( 1 );
  }

  auto tree = OwnedMutTree::allocate( 3 );
  tree.at( 0 ) = Handle( "unused" );
  tree.at( 1 ) = *compiler;
  tree.at( 2 ) = wasm;
  return thunk( rt.storage().add_tree( std::move( tree ) ) );
}

static Handle tree( std::initializer_list<Handle> elements )
{
  auto& rt = Runtime::get_instance();
  auto t = OwnedMutTree::allocate( static_cast<uint32_t>( elements.size() ) );
  size_t i = 0;
  for ( Handle element : elements ) {
    t.at( i++ ) = element;
  }
  return rt.storage().add_tree( std::move( t ) );
}

static Handle blob( std::string_view contents )
{
  auto& rt = Runtime::get_instance();
  auto blob = OwnedMutBlob::allocate( contents.size() );
  memcpy( blob.data(), contents.data(), contents.size() );
  return rt.storage().add_blob( std::move( blob ) );
}

static Handle file( std::filesystem::path path )
{
  auto& rt = Runtime::get_instance();
  OwnedBlob blob = OwnedBlob::from_file( path );
  return rt.storage().add_blob( std::move( blob ) );
}

static Handle flatware_input( Handle program,
                              Handle filesystem = tree( {} ),
                              Handle args = tree( {} ),
                              Handle stdin = blob( "" ),
                              Handle env = tree( {} ) )
{
  auto& rt = Runtime::get_instance();
  auto flatware_input = OwnedMutTree::allocate( 6 );
  flatware_input.at( 0 ) = Handle( "unused" );
  flatware_input.at( 1 ) = program;
  flatware_input.at( 2 ) = filesystem;
  flatware_input.at( 3 ) = args;
  flatware_input.at( 4 ) = stdin;
  flatware_input.at( 5 ) = env;
  return rt.storage().add_tree( std::move( flatware_input ) ).as_thunk();
}
