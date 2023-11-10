#pragma once

#include <bit>
#include <cassert>
#include <iostream>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "absl/hash/hash.h"

#include <experimental/bits/simd.h>
#include <immintrin.h>

#include "fixpointapi.hh"

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
 */
class Handle : public cookie
{
  friend class RuntimeWorker;
  friend __m256i fixpoint::create_tag( __m256i, __m256i );
  friend class RuntimeStorage;

public:
  Handle() = default;

  operator __m256i() const { return content_; }

  Handle( const __m256i val )
    : cookie( val )
  {}

  /* Cast 4*uint64_t to a Handle */
  Handle( uint64_t a, uint64_t b, uint64_t c, uint64_t d )
    : cookie( a, b, c, d )
  {}

  /* Cast 32 bytes to a Handle */
  Handle( const std::array<char, 32>& input )
    : cookie( input ) {};

  /* Construct a Handle out of a sha256 hash */
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
    if ( literal_content.size() ) {
      assert( literal_content.data() );
      __builtin_memcpy( (char*)&content_, literal_content.data(), literal_content.size() );
    }
    // set the handle to literal
    uint8_t metadata = 0x20 | literal_content.size();
    __builtin_memcpy( (char*)&content_ + 31, &metadata, 1 );
  }

  /* Construct a Handle out of literal blob content (as an integer) */
  Handle( uint32_t x )
  {
    // set the handle to literal
    uint8_t metadata = 0x20 | 4;
    __builtin_memcpy( (char*)&content_, &x, 4 );
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

  bool is_literal_blob() const { return metadata() & 0x20; }

  uint8_t literal_blob_len() const
  {
    assert( is_literal_blob() );
    return metadata() & 0x1f;
  }

  size_t get_length() const
  {
    if ( is_literal_blob() ) {
      return literal_blob_len();
    } else {
      return content_[2];
    }
  }

  bool is_canonical() const { return is_literal_blob() or metadata() & 0x04; }

  bool is_local() const { return !is_canonical(); }

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

  std::span<const char> literal_blob() const
  {
    assert( is_literal_blob() );
    return { data(), (size_t)literal_blob_len() };
  }

  size_t get_local_id() const { return _mm256_extract_epi64( content_, 0 ); }

  bool is_strict() const { return ( ( ( metadata() & 0xc0 ) >> 6 ) == static_cast<uint8_t>( Laziness::Strict ) ); }

  bool is_shallow() const
  {
    return ( ( ( metadata() & 0xc0 ) >> 6 ) == static_cast<uint8_t>( Laziness::Shallow ) );
  }

  bool is_lazy() const { return ( ( ( metadata() & 0xc0 ) >> 6 ) == static_cast<uint8_t>( Laziness::Lazy ) ); }

  Laziness get_laziness() const { return Laziness( ( metadata() & 0xc0 ) >> 6 ); }

  uint8_t get_metadata() { return metadata(); }

  static Handle name_only( Handle handle )
  {
    __m256i mask = _mm256_set_epi64x( 0xc0'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_andnot_si256( mask, handle );
  }

  static Handle get_thunk_name( const Handle handle )
  {
    // 00000001
    assert( handle.is_tree() );
    __m256i mask = _mm256_set_epi64x( 0x01'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_or_si256( mask, handle );
  }

  static Handle make_strict( const Handle handle )
  {
    // 00xxxxxx
    __m256i mask = _mm256_set_epi64x( 0xc0'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_andnot_si256( mask, handle );
  }

  static Handle make_shallow( const Handle handle )
  {
    // 01xxxxxx
    __m256i mask = _mm256_set_epi64x( 0xc0'00'00'00'00'00'00'00, 0, 0, 0 );
    __m256i masked = _mm256_andnot_si256( mask, handle );
    __m256i bits = _mm256_set_epi64x( 0x40'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_or_si256( bits, masked );
  }

  static Handle make_lazy( const Handle handle )
  {
    // 11xxxxxx
    auto mask = _mm256_set_epi64x( 0xc0'00'00'00'00'00'00'00, 0, 0, 0 );
    auto masked = _mm256_andnot_si256( mask, handle );
    auto bits = _mm256_set_epi64x( 0x80'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_or_si256( bits, masked );
  }

  static Handle make_tree( const Handle handle )
  {
    // xxxxxx00
    assert( handle.is_tag() or handle.is_tree() or handle.is_thunk() );
    __m256i mask = _mm256_set_epi64x( 0x03'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_andnot_si256( mask, handle );
  }

  static Handle make_tag( const Handle handle )
  {
    assert( handle.is_tree() );
    // xxxxxx11
    __m256i bits = _mm256_set_epi64x( 0x03'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_or_si256( bits, handle );
  }

  static Handle make_thunk( const Handle handle )
  {
    assert( handle.is_tree() );
    // xxxxxx01
    __m256i mask = _mm256_set_epi64x( 0x03'00'00'00'00'00'00'00, 0, 0, 0 );
    __m256i masked = _mm256_andnot_si256( mask, handle );
    __m256i bits = _mm256_set_epi64x( 0x01'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_or_si256( bits, masked );
  }

  Handle as_strict() const { return Handle::make_strict( *this ); }
  Handle as_shallow() const { return Handle::make_shallow( *this ); }
  Handle as_lazy() const { return Handle::make_lazy( *this ); }
  Handle with_laziness( Laziness laziness ) const
  {
    switch ( laziness ) {
      case Laziness::Lazy:
        return as_lazy();
      case Laziness::Shallow:
        return as_shallow();
      case Laziness::Strict:
        return as_strict();
    }
    __builtin_unreachable();
  }

  Handle as_tree() const { return Handle::make_tree( *this ); }
  Handle as_thunk() const { return Handle::make_thunk( *this ); }

private:
  Handle as_tag() const { return Handle::make_tag( *this ); }

public:
  Handle with_type( ContentType type ) const
  {
    if ( get_content_type() == type )
      return *this;
    switch ( type ) {
      case ContentType::Blob:
        assert( false );
      case ContentType::Tree:
        return as_tree();
      case ContentType::Tag:
        return as_tag();
      case ContentType::Thunk:
        return as_thunk();
    }
    __builtin_unreachable();
  }

  friend bool operator==( Handle lhs, Handle rhs );
  friend std::ostream& operator<<( std::ostream& s, const Handle name );

  friend struct IdentityHash;
  friend struct AbslHash;

  template<typename H>
  friend H AbslHashValue( H h, const Handle& handle )
  {
    return H::combine( std::move( h ), _mm256_extract_epi64( handle.content_, 0 ) );
  }
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

std::ostream& operator<<( std::ostream& s, const ContentType content_type );
std::ostream& operator<<( std::ostream& s, const Handle handle );

template<>
struct std::hash<Handle>
{
  std::size_t operator()( Handle const& handle ) const noexcept
  {
    return std::hash<uint64_t> {}( handle.get_local_id() );
  }
};

static_assert( sizeof( __m256i ) == sizeof( Handle ) );
