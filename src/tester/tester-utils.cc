#include <charconv>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <unistd.h>

#include "base16.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "object.hh"
#include "tester-utils.hh"
#include "tree.hh"
#include "types.hh"

using namespace std;

bool consumed = false;

static Handle<ValueTree> make_limits( IRuntime& rt, uint64_t memory, uint64_t output_size, uint64_t output_fanout )
{
  auto tree = OwnedMutTree::allocate( 3 );
  tree.at( 0 ) = Handle<Literal>( memory );
  tree.at( 1 ) = Handle<Literal>( output_size );
  tree.at( 2 ) = Handle<Literal>( output_fanout );
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
    .or_else( [&]() -> optional<Handle<Fix>> { throw std::runtime_error( "Not Identification-able" ); } )
    .value();
}

static Handle<Application> make_compile( IRuntime& rt, Handle<Fix> wasm )
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

Handle<AnyTree> make_tree( IRuntime& rt, OwnedMutTree data )
{
  return rt.create( std::make_shared<OwnedTree>( std::move( data ) ) );
}

template<FixHandle... Args>
Handle<AnyTree> make_tree( IRuntime& rt, Args... args )
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

  return make_tree( rt, std::move( tree ) );
}

static Handle<Blob> make_blob( IRuntime& rt, std::string_view contents )
{
  auto blob = OwnedMutBlob::allocate( contents.size() );
  memcpy( blob.data(), contents.data(), contents.size() );
  return rt.create( std::make_shared<OwnedBlob>( std::move( blob ) ) );
}

static Handle<Blob> make_blob( IRuntime& rt, std::filesystem::path path )
{
  return rt.create( std::make_shared<OwnedBlob>( path ) );
}

template<std::integral T>
T to_int( const std::string_view str );
template<std::integral T>
T from_int( const std::string_view str );

template<typename T>
Handle<Fix> make_blob( IRuntime& rt, T t ) requires std::is_integral_v<T>
{
  std::string_view s { (const char*)&t, sizeof( t ) };
  return make_blob( rt, s );
}

/**
 * Adds the args to RuntimeStorage, loading files and creating objects as necessary.
 * The contents of @p open_files must outlive this RuntimeStorage instance.
 */
Handle<Fix> parse_args( IRuntime& rt, std::span<char*>& args )
{
  if ( args.empty() ) {
    throw runtime_error( "not enough args" );
  }

  const string_view str { args[0] };

  if ( str.starts_with( "file:" ) ) {
    std::filesystem::path file( string( str.substr( 5 ) ) );
    args = args.subspan( 1 );
    return make_blob( rt, file );
  }

  if ( str.starts_with( "compile:" ) ) {
    std::filesystem::path file( string( str.substr( 8 ) ) );
    args = args.subspan( 1 );
    if ( consumed ) {
      return Handle<Strict>( make_compile( rt, make_blob( rt, file ) ) );
    } else {
      return make_compile( rt, make_blob( rt, file ) );
    }
  }

  if ( str.starts_with( "label:" ) ) {
    args = args.subspan( 1 );
    auto label = str.substr( 6 );
    if ( rt.contains( label ) ) {
      if ( consumed ) {
        auto id = make_identification( rt.labeled( label ) );
        return handle::extract<Identification>( id )
          .transform( []( auto h ) -> Handle<Fix> { return Handle<Strict>( h ); } )
          .or_else( [&]() -> optional<Handle<Fix>> { return id; } )
          .value();
      } else {
        return make_identification( rt.labeled( label ) );
      }
    } else {
      throw runtime_error( string( "Label not found: " ).append( str.substr( 6 ) ) );
    }
  }

  if ( str.starts_with( "name:" ) ) {
    args = args.subspan( 1 );
    if ( consumed ) {
      auto id = make_identification( Handle<Fix>::forge( base16::decode( str.substr( 5 ) ) ) );
      return handle::extract<Identification>( id )
        .transform( []( auto h ) -> Handle<Fix> { return Handle<Strict>( h ); } )
        .or_else( [&]() -> optional<Handle<Fix>> { return id; } )
        .value();
    } else {
      return make_identification( Handle<Fix>::forge( base16::decode( str.substr( 5 ) ) ) );
    }
  }

  if ( str.starts_with( "short-name:" ) ) {
    throw runtime_error( "Unimplemented" );
    // args.remove_prefix( 1 );
    // creturn rt.storage().get_full_name( string( str.substr( 11 ) ) );
  }

  if ( str.starts_with( "string:" ) ) {
    args = args.subspan( 1 );
    return make_blob( rt, str.substr( 7 ) );
  }

  if ( str.starts_with( "uint8:" ) ) {
    args = args.subspan( 1 );
    return make_blob( rt, to_int<uint8_t>( str.substr( 6 ) ) );
  }

  if ( str.starts_with( "uint16:" ) ) {
    args = args.subspan( 1 );
    return make_blob( rt, to_int<uint16_t>( str.substr( 7 ) ) );
  }

  if ( str.starts_with( "uint32:" ) ) {
    args = args.subspan( 1 );
    return make_blob( rt, to_int<uint32_t>( str.substr( 7 ) ) );
  }

  if ( str.starts_with( "uint64:" ) ) {
    args = args.subspan( 1 );
    return make_blob( rt, to_int<uint64_t>( str.substr( 7 ) ) );
  }

  if ( str.starts_with( "tree:" ) ) {
    const uint32_t tree_size = to_int<uint32_t>( str.substr( 5 ) );
    args = args.subspan( 1 );
    if ( args.size() < tree_size ) {
      throw runtime_error( "not enough args to make Tree of length " + to_string( tree_size ) );
    }

    OwnedMutTree the_tree = OwnedMutTree::allocate( tree_size );
    for ( uint32_t i = 0; i < tree_size; ++i ) {
      the_tree[i] = parse_args( rt, args );
    }
    return make_tree( rt, std::move( the_tree ) ).visit<Handle<Fix>>( []( auto h ) { return h; } );
  }

  if ( str.starts_with( "application:" ) ) {
    args = args.subspan( 1 );
    const string_view str1 { args[0] };
    if ( !str1.starts_with( "tree:" ) ) {
      throw runtime_error( "thunk not refering a tree" );
    }

    consumed = true;

    auto h = parse_args( rt, args );
    auto tree_name = handle::extract<ExpressionTree>( h )
                       .transform( []( auto t ) -> Handle<AnyTree> { return t; } )
                       .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ObjectTree>( h ); } )
                       .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ValueTree>( h ); } )
                       .transform( &handle::upcast );

    consumed = false;

    return Handle<Application>( tree_name.value() );
  }

  if ( str.starts_with( "strict:" ) ) {
    args = args.subspan( 1 );
    const string_view str1 { args[0] };
    if ( !str1.starts_with( "application:" ) ) {
      throw runtime_error( "encode not refering a thunk" );
    }

    auto h = parse_args( rt, args );
    auto thunk_name = handle::extract<Application>( h ).value();
    return Handle<Strict>( thunk_name );
  }

  throw runtime_error( "unknown object syntax: \"" + string( str ) + "\"" );
}

template<integral T>
T to_int( const string_view str )
{
  T ret {};

  const auto [ptr, ec] = from_chars( str.data(), str.data() + str.size(), ret );

  if ( ptr != str.data() + str.size() or ec != errc {} ) {
    throw system_error { make_error_code( ec ),
                         "to_int<uint" + to_string( 8 * sizeof( T ) ) + "_t>( \"" + string( str ) + "\" )" };
  }

  return ret;
}

template<integral T>
T from_int( const std::span<const char> str )
{
  T ret {};
  if ( sizeof( T ) != str.size() ) {
    throw runtime_error( "to_int( string_view ) size mismatch" );
  }

  memcpy( &ret, str.data(), sizeof( T ) );

  return ret;
}

void parser_usage_message()
{
  cerr << "Usage: entry...\n";
  cerr << "   entry :=   file:<filename>\n";
  cerr << "            | string:<string>\n";
  cerr << "            | name:<base16-encoded name>\n";
  // cerr << "            | short-name:<7 bytes shortened base16-encoded name>\n";
  cerr << "            | uint<n>:<integer> (with <n> = 8 | 16 | 32 | 64)\n";
  cerr << "            | tree:<n> (followed by <n> entries)\n";
  cerr << "            | application: (followed by tree:<n>)\n";
  cerr << "            | strict: (followed by application:)\n";
  cerr << "            | compile:<filename>\n";
  cerr << "            | label:<ref>\n";
}
