#pragma once

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
  Blob,
  Tree,
  Thunk
};

class Name
{
  __m256i content_ {};

  const char* data() const { return reinterpret_cast<const char*>( &content_ ); }

  uint8_t metadata() const { return _mm256_extract_epi8( content_, 31 ); }

public:
  Name() = default;

  Name( const __m256i val )
    : content_( val )
  {}

  Name( uint64_t a, uint64_t b, uint64_t c, uint64_t d )
    : content_( __m256i { int64_t( a ), int64_t( b ), int64_t( c ), int64_t( d ) } )
  {}

  Name( const std::array<char, 32>& input ) { __builtin_memcpy( &content_, input.data(), 32 ); }

  Name( std::string hash, ContentType content_type )
  {
    assert( hash.size() == 32 );
    hash[31] = static_cast<char>( 0x04 | static_cast<uint8_t>( content_type ) );
    __builtin_memcpy( &content_, hash.data(), 32 );
  }

  Name( const std::string& literal_content )
  {
    assert( literal_content.size() < 32 );
    uint8_t metadata = 0x40 | literal_content.size();
    std::array<char, 32> content {};
    __builtin_memcpy( &content, literal_content.data(), literal_content.size() );
    content[31] = metadata;
    __builtin_memcpy( &content_, content.data(), 32 );
  }

  bool is_strict_tree_entry() const { return !( metadata() & 0x80 ); }

  Name name_only() const
  {
    return __m256i { content_[0], content_[1], content_[2], content_[3] & 0x7f'ff'ff'ff'ff'ff'ff'ff };
  }

  operator __m256i() const { return content_; }

  bool is_literal_blob() const { return metadata() & 0x40; }

  uint8_t literal_blob_len() const
  {
    assert( is_literal_blob() );
    return metadata() & 0x1f;
  }

  bool is_canonical() const
  {
    assert( not is_literal_blob() );
    return metadata() & 0x04;
  }

  bool is_canonical_tree() const
  {
    assert( not is_literal_blob() );
    return metadata() & ( 0x04 | static_cast<uint8_t>( ContentType::Tree ) );
  }

  bool is_canonical_thunk() const
  {
    assert( not is_literal_blob() );
    return metadata() & ( 0x04 | static_cast<uint8_t>( ContentType::Thunk ) );
  }

  Name get_thunk_name() const
  {
    assert( is_canonical_tree() );
    return __m256i { content_[0],
                     content_[1],
                     content_[2],
                     ( content_[3] | 0x02'00'00'00'00'00'00'00 ) & ~( 0x01'00'00'00'00'00'00'00 ) };
  }

  Name get_encode_name() const
  {
    assert( is_canonical_thunk() );
    return __m256i { content_[0],
                     content_[1],
                     content_[2],
                     static_cast<int64_t>( ( content_[3] & ~( 0x02'00'00'00'00'00'00'00 ) )
                                           | 0x01'00'00'00'00'00'00'00 ) };
  }

  ContentType get_content_type() const
  {
    assert( not is_literal_blob() );
    return ContentType( metadata() & 0x03 );
  }

  std::string_view literal_blob() const
  {
    assert( is_literal_blob() );
    return { data(), literal_blob_len() };
  }

  void* local_name()
  {
    assert( not is_canonical() );
    return reinterpret_cast<void*>( content_[0] );
  }

  template<typename H>
  friend H AbslHashValue( H h, const Name& name )
  {
    return H::combine( std::move( h ), name.content_[0], name.content_[1], name.content_[2], name.content_[3] );
  }

  friend bool operator==( const Name& lhs, const Name& rhs );
  friend std::ostream& operator<<( std::ostream& s, const Name& name );
};

bool operator==( const Name& lhs, const Name& rhs );
std::ostream& operator<<( std::ostream& s, const Name& name );
