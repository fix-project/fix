#pragma once

#include "cookie.hh"
#include "name.hh"

/**
 * TreeEntry metadata:
 * if the tree is a MTree before freezing: | strict/lazy(intended) | other/literal | accessible/not
 * accessible(from original objectreference) | the same as underlying name otherwise, the same as Name
 */
class MTreeEntry : public cookie_name
{
public:
  MTreeEntry() = default;

  operator __m256i() const { return content_; }

  MTreeEntry( const __m256i val )
    : cookie_name( val )
  {}

  bool is_accessible() const { return !( metadata() & 0x20 ); }
};

using MBlob = wasm_rt_memory_t;
using MTree = wasm_rt_externref_table_t;

/**
 * MutableValueReference:
 * content_[0...30]: address of MBlob/ MTree
 * metadata: | 0 | 0 | 0 | 0 | 0 | MBlob/MTree | 1 | 1
 */
class MutableValueReference : public cookie_name
{
public:
  MutableValueReference() = default;
  MutableValueReference( void* ptr, bool is_mblob )
  {
    uint8_t metadata = 0x03 | is_mblob ? 0x04 : 0x00;
    std::array<char, 32> content {};
    __builtin_memcpy( &content, &ptr, 8 );
    content[31] = metadata;
    __builtin_memcpy( &content_, content.data(), 32 );
  }

  MutableValueReference( const __m256i val )
    : cookie_name( val )
  {}

  operator __m256i() const { return content_; }

  bool is_mblob() const { return metadata() & 0x04; }

  bool is_mtree() const { return !is_mblob(); }

  MBlob* get_mblob_ptr() const
  {
    assert( is_mblob() );
    return reinterpret_cast<MBlob*>( _mm256_extract_epi64( content_, 0 ) );
  }

  MTree* get_mtree_ptr() const
  {
    assert( is_mtree() );
    return reinterpret_cast<MTree*>( _mm256_extract_epi64( content_, 0 ) );
  }
};
