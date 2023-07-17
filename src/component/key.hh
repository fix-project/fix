#pragma once

#include <bit>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "absl/hash/hash.h"
#include "job.hh"

#include <experimental/bits/simd.h>
#include <immintrin.h>

#include <cassert>

#include <handle.hh>

const size_t OPERATION_SIZE = 4;

/**
 * Key 8-bit metadata:
 * 1) if the handle is a literal:     | strict/shallow/lazy (2 bits) | other/literal | size of the blob (5 bits)
 * 2) if the handle is not a literal: | strict/shallow/lazy (2 bits) | other/literal | 0 | 0 | 0 |
 * Blob/Tree/Thunk/Tag (2 bits)
 *
 * Key 256-bits:
 * content_[0]  |  content_[1]  |  content_[2]  |  content_[3][0:31]  |  content_[3][56:63]
 *   local_id   |  multimap idx |     size      |      zeroed out     |         metadata
 */

class Key : public cookie
{
public:
  Key() = default;

  operator __m256i() const { return content_; }

  Key( const __m256i val )
    : cookie( val )
  {}

  Key( uint64_t a, uint64_t b, uint64_t c, uint64_t d )
    : cookie( a, b, c, d )
  {}

  /* Construct a Key out of literal blob content */
  Key( std::string_view literal_content )
  {
    assert( literal_content.size() < 32 );
    // set the handle to literal
    uint8_t metadata = 0x20 | literal_content.size();
    __builtin_memcpy( (char*)&content_, literal_content.data(), literal_content.size() );
    __builtin_memcpy( (char*)&content_ + 31, &metadata, 1 );
  }

  /* Construct a Key out of local id */
  Key( size_t local_id, size_t size, ContentType content_type )
  {
    uint8_t metadata = static_cast<uint8_t>( content_type );
    content_[0] = local_id;
    content_[2] = size;
    __builtin_memcpy( (char*)&content_ + 31, &metadata, 1 );
  }

  /* Construct a job handle */
  Key( Key handle, bool pending, std::initializer_list<uint8_t> list )
    : cookie( handle )
  {
    // pending:
    // 0, list0, list1, list2
    // not pending:
    // list2, list1, list0, 0
    if ( pending ) {
      for ( auto elem : list ) {
        rotate_operations_left();
        operations_[3] = elem;
      }
    } else {
      for ( auto elem : list ) {
        rotate_operations_right();
        operations_[0] = elem;
      }
    }
  }

  Key( Handle handle )
    : cookie( handle )
  {
    for ( size_t i = 0; i < OPERATION_SIZE; i++ ) {
      uint32_t operation = handle.pop_operation();
      operations_[i] = operation;
    }
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

  bool is_local() const { return !is_literal_blob(); }

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
    uint32_t prev = operations_[OPERATION_SIZE - 1];
    rotate_operations_right();
    return prev;
  }

  uint32_t peek_operation() { return operations_[OPERATION_SIZE - 1]; }

  static Key name_only( Key handle )
  {
    __m256i mask = _mm256_set_epi64x( 0xc0'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_andnot_si256( mask, handle );
  }

  static Key get_thunk_name( const Key handle )
  {
    // 00000001
    assert( handle.is_tree() );
    __m256i mask = _mm256_set_epi64x( 0x01'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_or_si256( mask, handle );
  }

  static Key get_encode_name( const Key handle )
  {
    assert( handle.is_thunk() );
    __m256i mask = _mm256_set_epi64x( 0x01'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_andnot_si256( mask, handle );
  }

  static Key make_shallow( const Key handle )
  {
    if ( ( handle.is_tree() || handle.is_tag() ) && handle.is_strict() ) {
      // 01000000
      __m256i mask = _mm256_set_epi64x( 0x40'00'00'00'00'00'00'00, 0, 0, 0 );
      return _mm256_or_si256( mask, handle );
    } else {
      return handle;
    }
  }

  friend bool operator==( Key lhs, Key rhs );
  friend std::ostream& operator<<( std::ostream& s, const Key name );

  template<typename H>
  friend H AbslHashValue( H h, const Key& key )
  {
    return H::combine( std::move( h ),
                       key.get_local_id(),
                       key.operations_[0],
                       key.operations_[1],
                       key.operations_[2],
                       key.operations_[3] );
  }

private:
  uint8_t operations_[OPERATION_SIZE];

  void rotate_operations_left()
  {
    uint8_t left_most = operations_[0];
    operations_[0] = operations_[1];
    operations_[1] = operations_[2];
    operations_[2] = operations_[3];
    operations_[3] = left_most;
  }

  void rotate_operations_right()
  {
    uint8_t right_most = operations_[3];
    operations_[3] = operations_[2];
    operations_[2] = operations_[1];
    operations_[1] = operations_[0];
    operations_[0] = right_most;
  }
};

inline bool operator==( Key lhs, Key rhs )
{
  __m256i pxor = _mm256_xor_si256( lhs, rhs );
  return _mm256_testz_si256( pxor, pxor );
}

std::ostream& operator<<( std::ostream& s, const Key handle );
