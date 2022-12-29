#pragma once

#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "absl/hash/hash.h"
#include "cookie.hh"

#include <experimental/bits/simd.h>
#include <immintrin.h>

#include <cassert>

class Name : public cookie_name
{
  /**
   * Name 8-bit metadata:
   * 1) if the name is a literal:     | strict/lazy | other/literal | 0 | size of the blob (5 bits)
   * 2) if the name is not a literal: | strict/lazy | other/literal | 0 | 0 | 0 | canonical/local | Blob/Tree/Thunk
   * (2 bits)
   */

public:
  Name() = default;

  operator __m256i() const { return content_; }

  Name( const __m256i val )
    : cookie_name( val )
  {}

  Name( uint64_t a, uint64_t b, uint64_t c, uint64_t d )
    : cookie_name( a, b, c, d )
  {}

  Name( const std::array<char, 32>& input )
    : cookie_name( input ) {};

  Name( std::string hash, size_t size, ContentType content_type )
  {
    assert( hash.size() == 32 );
    hash[31] = static_cast<char>( 0x04 | static_cast<uint8_t>( content_type ) );
    __builtin_memcpy( &content_, hash.data(), 32 );
    content_[2] = size;
  }

  Name( std::string_view literal_content )
  {
    assert( literal_content.size() < 32 );
    uint8_t metadata = 0x40 | literal_content.size();
    __builtin_memcpy( (char*)&content_, literal_content.data(), literal_content.size() );
    __builtin_memcpy( (char*)&content_ + 31, &metadata, 1 );
  }

  Name( size_t local_id, size_t size, ContentType content_type )
  {
    uint8_t metadata = static_cast<uint8_t>( content_type );
    content_[0] = local_id;
    content_[2] = size;
    __builtin_memcpy( (char*)&content_ + 31, &metadata, 1 );
  }

  size_t get_local_id() const { return _mm256_extract_epi64( content_, 0 ); };

  bool is_strict_tree_entry() const { return !( metadata() & 0x80 ); }

  static Name name_only( Name name )
  {
    __m256i mask = _mm256_set_epi64x( 0x80'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_or_si256( mask, name );
  }

  static Name get_thunk_name( Name name )
  {
    assert( name.is_tree() );
    __m256i mask = _mm256_set_epi64x( 0x01'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_or_si256( mask, name );
  }

  static Name get_encode_name( Name name )
  {
    assert( name.is_thunk() );
    __m256i mask = _mm256_set_epi64x( 0x01'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_andnot_si256( mask, name );
  }

  friend bool operator==( Name lhs, Name rhs );
  friend std::ostream& operator<<( std::ostream& s, const Name name );

  template<typename H>
  friend H AbslHashValue( H h, const Name& name )
  {
    return H::combine( std::move( h ), _mm256_extract_epi64( name.content_, 0 ) );
  }

  friend struct NameHash;
  friend struct AbslHash;
};

struct NameHash
{
  std::size_t operator()( Name const& name ) const noexcept { return _mm256_extract_epi64( name.content_, 0 ); }
};

struct AbslHash
{
  std::size_t operator()( Name const& name ) const noexcept
  {
    return absl::Hash<uint64_t> {}( _mm256_extract_epi64( name.content_, 0 ) );
  }
};

inline bool operator==( Name lhs, Name rhs )
{
  __m256i pxor = _mm256_xor_si256( lhs, rhs );
  return _mm256_testz_si256( pxor, pxor );
}

std::ostream& operator<<( std::ostream& s, const Name name );
