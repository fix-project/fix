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
  {
  }

  Name( uint64_t a, uint64_t b, uint64_t c, uint64_t d )
    : cookie_name( a, b, c, d )
  {
  }

  Name( const std::array<char, 32>& input )
    : cookie_name( input ) {};

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
    __builtin_memcpy( (char*)&content_, literal_content.data(), literal_content.size() );
    __builtin_memcpy( (char*)&content_ + 31, &metadata, 1 );
  }

  Name( uint32_t local_id, ContentType content_type )
  {
    uint8_t metadata = static_cast<uint8_t>( content_type );
    __builtin_memcpy( (char*)&content_, &local_id, sizeof( uint32_t ) );
    __builtin_memcpy( (char*)&content_ + 31, &metadata, 1 );
  }

  bool is_strict_tree_entry() const { return !( metadata() & 0x80 ); }

  static Name name_only( Name name )
  {
    return __m256i {
      name.content_[0], name.content_[1], name.content_[2], name.content_[3] & 0x7f'ff'ff'ff'ff'ff'ff'ff
    };
  }

  static Name get_thunk_name( Name name )
  {
    assert( name.is_tree() );
    return __m256i { name.content_[0],
                     name.content_[1],
                     name.content_[2],
                     ( name.content_[3] | 0x02'00'00'00'00'00'00'00 ) & ~( 0x01'00'00'00'00'00'00'00 ) };
  }

  static Name get_encode_name( Name name )
  {
    assert( name.is_thunk() );
    return __m256i { name.content_[0],
                     name.content_[1],
                     name.content_[2],
                     static_cast<int64_t>( ( name.content_[3] & ~( 0x02'00'00'00'00'00'00'00 ) )
                                           | 0x01'00'00'00'00'00'00'00 ) };
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
