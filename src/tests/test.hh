#pragma once

#include "runtimes.hh"

#pragma GCC diagnostic ignored "-Wunused-function"

static Handle<Fix> make_identification( Handle<Fix> name )
{
  return handle::extract<Value>( name )
    .transform( [&]( auto h ) -> Handle<Fix> {
      return h.template visit<Handle<Fix>>(
        overload { []( Handle<Blob> b ) { return Handle<Identification>( b ); },
                   []( Handle<ValueTree> t ) { return Handle<Identification>( t ); },
                   [&]( auto ) { return name; } } );
    } )
    .or_else( [&]() -> std::optional<Handle<Fix>> { throw std::runtime_error( "Not Identifiable" ); } )
    .value();
}

static Handle<Strict> compile( FrontendRT& rt, Handle<Fix> wasm )
{
  auto compiler = rt.labeled( "compile-encode" );

  auto tree = OwnedMutTree::allocate( 3 );
  tree.at( 0 ) = Handle<Literal>( "unused" );
  tree.at( 1 ) = Handle<Strict>( handle::extract<Identification>( make_identification( compiler ) ).value() );
  tree.at( 2 ) = wasm;
  return rt.create( std::make_shared<OwnedTree>( std::move( tree ) ) ).visit<Handle<Strict>>( []( auto x ) {
    return Handle<Strict>( Handle<Application>( Handle<ExpressionTree>( x ) ) );
  } );
}

template<FixHandle... Args>
Handle<AnyTree> tree( FrontendRT& rt, Args... args )
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

static Handle<Blob> blob( FrontendRT& rt, std::string_view contents )
{
  auto blob = OwnedMutBlob::allocate( contents.size() );
  memcpy( blob.data(), contents.data(), contents.size() );
  return rt.create( std::make_shared<OwnedBlob>( std::move( blob ) ) );
}

static Handle<Blob> file( FrontendRT& rt, std::filesystem::path path )
{
  return rt.create( std::make_shared<OwnedBlob>( path ) );
}

static Handle<Eval> flatware_input( FrontendRT& rt,
                                    Handle<Fix> program,
                                    std::optional<Handle<Fix>> filesystem = std::nullopt,
                                    std::optional<Handle<Fix>> args = std::nullopt,
                                    std::optional<Handle<Blob>> stdin = std::nullopt,
                                    std::optional<Handle<Fix>> env = std::nullopt )
{
  auto input_tree = tree( rt,
                          Handle<Literal>( "unused" ),
                          program,
                          filesystem.value_or( handle::upcast( tree( rt ) ) ),
                          args.value_or( handle::upcast( tree( rt ) ) ),
                          stdin.value_or( blob( rt, "" ) ),
                          env.value_or( handle::upcast( tree( rt ) ) ) );
  return Handle<Eval>( Handle<Application>( handle::upcast( input_tree ) ) );
}
