#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "types.hh"

template<>
struct Handle<Named>
{
  u8x32 content;

private:
  inline Handle( const u8x32 content )
    : content( content )
  {}

public:
  inline Handle( u8x32 hash, uint64_t size )
    : content( hash )
  {
    assert( ( size & 0xffff000000000000 ) == 0 );
    assert( size > Handle<Literal>::MAXIMUM_LENGTH );
    ( *(u64x4*)&content )[3] = size;
  }

  inline size_t size() const
  {
    return ( (unsigned long long __attribute__( ( vector_size( 32 ) ) ))content )[3] & 0xffffffffffff;
  }
  inline bool empty() const { return size() == 0; }
  inline bool is_local() const { return ( content[30] >> 7 ) & 1; }

  static inline Handle<Named> forge( u8x32 content ) { return { content }; }
};

template<>
struct Handle<Literal>
{
  u8x32 content;

private:
  inline Handle( const unsigned char __attribute__( ( vector_size( 32 ) ) ) content )
    : content( content )
  {}

public:
  static constexpr size_t MAXIMUM_LENGTH = 30;

  inline Handle( const std::string_view str )
    : content {}
  {
    assert( str.size() <= 30 );
    memcpy( &content, str.data(), str.size() );
    content[30] = str.size() << 3;
  }

  template<typename T>
  requires std::integral<T> or std::floating_point<T>
  inline Handle( const T x )
    : content {}
  {
    static_assert( sizeof( T ) <= MAXIMUM_LENGTH );
    memcpy( &content, &x, sizeof( T ) );
    content[30] = sizeof( T ) << 3;
  }

  inline static Handle zero()
  {
    const u8x32 zero { 0 };
    return forge( zero );
  }

  inline unsigned char size() const { return content[30] >> 3; }
  inline bool empty() const { return size() == 0; }

  inline const char* data() const { return (const char*)&content; }
  inline const std::string_view view() const { return { data(), size() }; }

  static inline Handle<Literal> forge( u8x32 content ) { return { content }; }

  inline operator std::string() const { return std::string( view() ); }

  template<typename T>
  requires std::integral<T> or std::floating_point<T>
  inline operator T() const
  {
    assert( sizeof( T ) == size() );
    return *( (T*)&content );
  }
};