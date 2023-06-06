#pragma once

#include <bit>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "absl/hash/hash.h"

#include <experimental/bits/simd.h>
#include <immintrin.h>

#include <cassert>

enum class ContentType : uint8_t
{
  Tree,
  Thunk,
  Blob,
  Tag
};

enum class Laziness : uint8_t
{
  Strict,
  Shallow,
  Lazy
};

class cookie
{
protected:
  __m256i content_ {};

  cookie() = default;

  cookie( const __m256i val )
    : content_( val )
  {}

  operator __m256i() const { return content_; }

  const char* data() const { return reinterpret_cast<const char*>( &content_ ); }

  uint8_t metadata() const { return _mm256_extract_epi8( content_, 31 ); }

  cookie( uint64_t a, uint64_t b, uint64_t c, uint64_t d )
    : content_( __m256i { int64_t( a ), int64_t( b ), int64_t( c ), int64_t( d ) } )
  {}

  cookie( const std::array<char, 32>& input ) { __builtin_memcpy( &content_, input.data(), 32 ); }
};

/**
 * Handle 8-bit metadata:
 * 1) if the handle is a literal:     | strict/shallow/lazy (2 bits) | other/literal | size of the blob (5 bits)
 * 2) if the handle is not a literal: | strict/shallow/lazy (2 bits) | other/literal | 0 | 0 | canonical/local |
 * Blob/Tree/Thunk/Tag (2 bits)
 *
 * Handle 256-bits local:
 * content_[0]  |  content_[1]  |  content_[2]  |  content_[3][0:31]  |  content_[3][56:63]
 *   local_id   |  multimap idx |     size      |      operations     |         metadata
 */

class Handle : public cookie
{
public:
  Handle() = default;

  operator __m256i() const { return content_; }

  Handle( const __m256i val )
    : cookie( val )
  {}

  Handle( uint64_t a, uint64_t b, uint64_t c, uint64_t d )
    : cookie( a, b, c, d )
  {}

  Handle( const std::array<char, 32>& input )
    : cookie( input ) {};

  /* Construct a Handle out ot a sha256 hash */
  Handle( std::string hash, size_t size, ContentType content_type )
  {
    assert( hash.size() == 32 );
    // set the handle to canonical
    hash[31] = static_cast<char>( 0x04 | static_cast<uint8_t>( content_type ) );
    __builtin_memcpy( &content_, hash.data(), 32 );
    content_[2] = size;
  }

  /* Construct a Handle out of literal blob content */
  Handle( std::string_view literal_content )
  {
    assert( literal_content.size() < 32 );
    // set the handle to literal
    uint8_t metadata = 0x20 | literal_content.size();
    __builtin_memcpy( (char*)&content_, literal_content.data(), literal_content.size() );
    __builtin_memcpy( (char*)&content_ + 31, &metadata, 1 );
  }

  /* Construct a Handle out of local id */
  Handle( size_t local_id, size_t size, ContentType content_type )
  {
    uint8_t metadata = static_cast<uint8_t>( content_type );
    content_[0] = local_id;
    content_[2] = size;
    __builtin_memcpy( (char*)&content_ + 31, &metadata, 1 );
  }

  /* Construct a job handle */
  Handle( Handle handle, bool pending, std::initializer_list<uint64_t> list )
    : cookie( handle )
  {
    uint32_t ops = 0;

    for ( auto elem : list ) {
      ops ^= elem;
      ops = std::rotl( (uint32_t)ops, ( pending ? 1 : -1 ) * 4 );
    }

    if ( pending ) {
      ops = std::rotr( (uint32_t)ops, 4 );
    }

    __builtin_memcpy( (char*)&content_ + 24, &ops, 4 );
  }

  bool is_literal_blob() const { return metadata() & 0x20; }

  uint8_t literal_blob_len() const
  {
    assert( is_literal_blob() );
    return metadata() & 0x1f;
  }

  size_t get_size() const
  {
    if ( is_literal_blob() ) {
      return literal_blob_len();
    } else {
      return content_[2];
    }
  }

  bool is_canonical() const { return !is_literal_blob() && metadata() & 0x04; }

  bool is_local() const { return !is_literal_blob() && !is_canonical(); }

  bool is_blob() const
  {
    return ( is_literal_blob() || ( ( metadata() & 0x03 ) == static_cast<uint8_t>( ContentType::Blob ) ) );
  }

  bool is_tree() const
  {
    return ( !is_literal_blob() && ( metadata() & 0x03 ) == static_cast<uint8_t>( ContentType::Tree ) );
  }

  bool is_thunk() const
  {
    return ( !is_literal_blob() && ( metadata() & 0x03 ) == static_cast<uint8_t>( ContentType::Thunk ) );
  }

  bool is_tag() const
  {
    return ( !is_literal_blob() && ( metadata() & 0x03 ) == static_cast<uint8_t>( ContentType::Tag ) );
  }

  ContentType get_content_type() const
  {
    if ( is_literal_blob() ) {
      return ContentType::Blob;
    } else {
      return ContentType( metadata() & 0x03 );
    }
  }

  std::string_view literal_blob() const
  {
    assert( is_literal_blob() );
    return { data(), literal_blob_len() };
  }

  size_t get_local_id() const { return _mm256_extract_epi64( content_, 0 ); }

  bool is_strict() const { return ( ( metadata() & 0xc0 ) == static_cast<uint8_t>( Laziness::Strict ) ); }

  bool is_shallow() const { return ( ( metadata() & 0xc0 ) == static_cast<uint8_t>( Laziness::Shallow ) ); }

  bool is_lazy() const { return ( ( metadata() & 0xc0 ) == static_cast<uint8_t>( Laziness::Lazy ) ); }

  uint8_t get_metadata() { return metadata(); }

  void set_index( size_t value ) { content_[1] = value; }

  uint32_t pop_operation()
  {
    uint32_t prev = 0xF & ( (uint32_t)content_[3] );
    uint32_t curr = std::rotr( (uint32_t)content_[3], 4 );

    __builtin_memcpy( (char*)&content_ + 24, &curr, 4 );

    return prev;
  }

  uint32_t peek_operation()
  {
    uint32_t prev = 0xF & ( (uint32_t)content_[3] );
    return prev;
  }

  static Handle name_only( Handle handle )
  {
    __m256i mask = _mm256_set_epi64x( 0xc0'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_andnot_si256( mask, handle );
  }

  static Handle get_thunk_name( const Handle handle )
  {
    assert( handle.is_tree() );
    __m256i mask = _mm256_set_epi64x( 0x01'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_or_si256( mask, handle );
  }

  static Handle get_encode_name( const Handle handle )
  {
    assert( handle.is_thunk() );
    __m256i mask = _mm256_set_epi64x( 0x01'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_andnot_si256( mask, handle );
  }

  friend bool operator==( Handle lhs, Handle rhs );
  friend std::ostream& operator<<( std::ostream& s, const Handle name );

  friend struct IdentityHash;
  friend struct AbslHash;
};

struct IdentityHash
{
  std::size_t operator()( Handle const& handle ) const noexcept
  {
    return _mm256_extract_epi64( handle.content_, 0 );
  }
};

struct AbslHash
{
  std::size_t operator()( Handle const& handle ) const noexcept
  {
    return absl::Hash<uint64_t> {}( _mm256_extract_epi64( handle.content_, 0 ) );
  }
};

inline bool operator==( Handle lhs, Handle rhs )
{
  __m256i pxor = _mm256_xor_si256( lhs, rhs );
  return _mm256_testz_si256( pxor, pxor );
}

std::ostream& operator<<( std::ostream& s, const Handle handle );
