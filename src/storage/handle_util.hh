#pragma once
#include <cwchar>
#include <string_view>

#include "blake3.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "object.hh"
#include "overload.hh"
#include "types.hh"

namespace handle {
static inline Handle<Blob> create( const BlobData& blob )
{
  if ( blob->size() <= Handle<Literal>::MAXIMUM_LENGTH ) {
    return Handle<Literal>( { blob->span().data(), blob->size() } );
  }
  u8x32 hash = blake3::encode( std::as_bytes( blob->span() ) );
  return Handle<Named> { hash, blob->size() };
}

static inline FixKind tree_kind( const TreeData& tree )
{
  FixKind kind = FixKind::Value;
  for ( size_t i = 0; i < tree->size(); i++ ) {
    if ( handle::kind( tree->at( i ) ) > kind ) {
      kind = handle::kind( tree->at( i ) );
    }
  }
  return kind;
}

static inline size_t tree_size( const TreeData& tree )
{
  size_t size = 0;
  for ( size_t i = 0; i < tree->size(); i++ ) {
    size += byte_size( tree->at( i ) );
  }
  size += tree->size() * sizeof( Handle<Fix> );
  return size;
}

static inline Handle<AnyTree> create( const TreeData& data )
{
  u8x32 hash = blake3::encode( std::as_bytes( data->span() ) );
  switch ( tree_kind( data ) ) {
    case FixKind::Value:
      return Handle<ValueTree>( hash, tree_size( data ) );
    case FixKind::Object:
      return Handle<ObjectTree>( hash, tree_size( data ) );
    case FixKind::Expression:
      return Handle<ExpressionTree>( hash, tree_size( data ) );
    case FixKind::Fix:
      throw std::runtime_error( "invalid contents of tree" );
  }
  __builtin_unreachable();
}

struct tree_equal
{
  constexpr bool operator()( const Handle<ExpressionTree>& lhs, const Handle<ExpressionTree>& rhs ) const
  {
    static constexpr u64x4 mask
      = { 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffff000000000000 };
    u64x4 pxor = (u64x4)( lhs.content ^ rhs.content ) & mask;
#ifdef __AVX__
    return _mm256_testz_si256( (__m256i)pxor, (__m256i)pxor );
#else
    return ( xored[0] | xored[1] | xored[2] | xored[3] ) == 0;
#endif
  }
};

struct any_tree_equal
{
  constexpr bool operator()( const Handle<AnyTree>& lhs, const Handle<AnyTree>& rhs ) const
  {
    static constexpr u64x4 mask
      = { 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0x0000000000000000 };
    u64x4 pxor = (u64x4)( lhs.content ^ rhs.content ) & mask;
#ifdef __AVX__
    return _mm256_testz_si256( (__m256i)pxor, (__m256i)pxor );
#else
    return ( xored[0] | xored[1] | xored[2] | xored[3] ) == 0;
#endif
  }
};
}

namespace job {
static inline Handle<Fix> get_root( Handle<Dependee> job )
{
  return job.visit<Handle<Fix>>( overload { [&]( Handle<Relation> r ) {
                                             return r.visit<Handle<Fix>>(
                                               overload { [&]( Handle<Step> s ) { return s.unwrap<Thunk>(); },
                                                          [&]( Handle<Eval> e ) { return e.unwrap<Object>(); } } );
                                           },
                                            [&]( auto h ) { return h; } } );
}
}
