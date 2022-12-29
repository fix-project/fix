#pragma once

#include <experimental/bits/simd.h>
#include <immintrin.h>

#include <cassert>

enum class ContentType : uint8_t
{
  Tree,
  Thunk,
  Blob
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
 * cookie_name metadata:
 * if literal: | _ | other/literal | _ | size of the blob (5 bits)
 * if not literal:  | _ | other/literal | _ | 0 | 0 | canonical/local | Blob/Tree/Thunk (2 bits)
 * ( _ means the bit is not used/has no required value)
 */
class cookie_name : public cookie
{
protected:
  cookie_name() = default;
  cookie_name( const __m256i val )
    : cookie( val )
  {}
  cookie_name( uint64_t a, uint64_t b, uint64_t c, uint64_t d )
    : cookie( a, b, c, d )
  {}
  cookie_name( const std::array<char, 32>& input )
    : cookie( input )
  {}

public:
  bool is_literal_blob() const { return metadata() & 0x40; }

  uint8_t literal_blob_len() const
  {
    assert( is_literal_blob() );
    return metadata() & 0x1f;
  }

  size_t get_size() const
  {
    assert( !is_literal_blob() );
    return content_[2];
  }

  bool is_canonical() const
  {
    assert( not is_literal_blob() );
    return metadata() & 0x04;
  }

  bool is_local() const
  {
    assert( not is_literal_blob() );
    return !is_canonical();
  }

  bool is_blob() const
  {
    return ( is_literal_blob() || ( ( metadata() & 0x03 ) == static_cast<uint8_t>( ContentType::Blob ) ) );
  }

  bool is_tree() const
  {
    assert( not is_literal_blob() );
    return ( !is_literal_blob() && ( metadata() & 0x03 ) == static_cast<uint8_t>( ContentType::Tree ) );
  }

  bool is_thunk() const
  {
    assert( not is_literal_blob() );
    return ( !is_literal_blob() && ( metadata() & 0x03 ) == static_cast<uint8_t>( ContentType::Thunk ) );
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

  void* local_name()
  {
    assert( not is_canonical() );
    return reinterpret_cast<void*>( content_[0] );
  }
};
