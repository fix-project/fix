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
protected:
  __m256i content_ {};

  const char* data() const { return reinterpret_cast<const char*>( &content_ ); }

  /**
   * Name 8-bit metadata:
   * 1) if the name is a literal:     | strict/lazy | other/literal | 0 | size of the blob (5 bits)
   * 2) if the name is not a literal: | strict/lazy | other/literal | 0 | 0 | 0 | 0 | canonical/not |
   * Blob/Tree/Thunk (2 bits)
   *
   * ObjectReference metadata:
   * | reffed/not reffed | other/literal | accessible/not assessible | the same as underlying name
   */
  uint8_t metadata() const { return _mm256_extract_epi8( content_, 31 ); }

public:
  Name() = default;

  operator __m256i() const { return content_; }

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

  ContentType get_content_type() const
  {
    if ( is_literal_blob() ) {
      return ContentType::Blob;
    } else {
      return ContentType( metadata() & 0x03 );
    }
  }

  bool is_strict_tree_entry() const { return !( metadata() & 0x80 ); }

  bool is_literal_blob() const { return metadata() & 0x40; }

  uint8_t literal_blob_len() const
  {
    assert( is_literal_blob() );
    return metadata() & 0x1f;
  }

  std::string_view literal_blob() const
  {
    assert( is_literal_blob() );
    return { data(), literal_blob_len() };
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

  bool is_reffed() const { return ( metadata() & 0x80 ); }

  bool is_accessible() const { return !( metadata() & 0x20 ); }

  static Name name_only( Name name )
  {
    return __m256i {
      name.content_[0], name.content_[1], name.content_[2], name.content_[3] & 0x7f'ff'ff'ff'ff'ff'ff'ff
    };
  }

  static Name object_reference_name_only( Name name )
  {
    return __m256i {
      name.content_[0], name.content_[1], name.content_[2], name.content_[3] & 0x5f'ff'ff'ff'ff'ff'ff'ff
    };
  }

  static Name ref_object_reference( Name name )
  {
    return __m256i { name.content_[0],
                     name.content_[1],
                     name.content_[2],
                     static_cast<int64_t>( name.content_[3] | 0x80'00'00'00'00'00'00'00 ) };
  }

  static Name unref_object_reference( Name name )
  {
    return __m256i {
      name.content_[0], name.content_[1], name.content_[2], name.content_[3] & 0x7f'ff'ff'ff'ff'ff'ff'ff
    };
  }

  static Name get_thunk_name( Name name )
  {
    assert( name.is_canonical_tree() );
    return __m256i { name.content_[0],
                     name.content_[1],
                     name.content_[2],
                     ( name.content_[3] | 0x02'00'00'00'00'00'00'00 ) & ~( 0x01'00'00'00'00'00'00'00 ) };
  }

  static Name get_encode_name( Name name )
  {
    assert( name.is_canonical_thunk() );
    return __m256i { name.content_[0],
                     name.content_[1],
                     name.content_[2],
                     static_cast<int64_t>( ( name.content_[3] & ~( 0x02'00'00'00'00'00'00'00 ) )
                                           | 0x01'00'00'00'00'00'00'00 ) };
  }

  static Name get_object_reference( Name name, bool accessible )
  {
    Name name_only = Name::name_only( name );
    if ( !accessible ) {
      return __m256i { name_only.content_[0],
                       name_only.content_[1],
                       name_only.content_[2],
                       name_only.content_[3] | 0x20'00'00'00'00'00'00'00 };
    } else {
      return name.content_;
    }
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
