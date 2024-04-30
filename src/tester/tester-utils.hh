#pragma once

#include "handle.hh"
#include "interface.hh"

Handle<Fix> parse_args( IRuntime& rt, std::span<char*>& args );
void parser_usage_message();

static Handle<ValueTree> make_limits( IRuntime& rt,
                                      uint64_t memory,
                                      uint64_t output_size,
                                      uint64_t output_fanout,
                                      bool fast = false )
{
  auto tree = OwnedMutTree::allocate( 4 );
  tree.at( 0 ) = Handle<Literal>( memory );
  tree.at( 1 ) = Handle<Literal>( output_size );
  tree.at( 2 ) = Handle<Literal>( output_fanout );
  tree.at( 3 ) = Handle<Literal>( (uint8_t)fast );
  auto created = rt.create( std::make_shared<OwnedTree>( std::move( tree ) ) );
  return handle::extract<ValueTree>( created ).value();
}

static Handle<Fix> make_identification( Handle<Fix> name )
{
  return handle::extract<Value>( name )
    .transform( [&]( auto h ) -> Handle<Fix> {
      return h.template visit<Handle<Fix>>(
        overload { []( Handle<Blob> b ) { return Handle<Identification>( b ); },
                   []( Handle<ValueTree> t ) { return Handle<Identification>( t ); },
                   [&]( auto ) { return name; } } );
    } )
    .or_else( [&]() -> std::optional<Handle<Fix>> { throw std::runtime_error( "Not Identification-able" ); } )
    .value();
}

[[maybe_unused]] static Handle<Application> make_compile( IRuntime& rt, Handle<Fix> wasm )
{
  auto compiler = rt.labeled( "compile-encode" );

  auto tree = OwnedMutTree::allocate( 3 );
  tree.at( 0 ) = make_limits( rt, 1024 * 1024 * 1024, 1024 * 1024, 1 );
  tree.at( 1 ) = Handle<Strict>( handle::extract<Identification>( make_identification( compiler ) ).value() );
  tree.at( 2 ) = wasm;
  return rt.create( std::make_shared<OwnedTree>( std::move( tree ) ) ).visit<Handle<Application>>( []( auto x ) {
    return Handle<Application>( Handle<ExpressionTree>( x ) );
  } );
}
