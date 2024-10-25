#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

#include "types.hh"

template<>
struct Handle<Literal>
{
  constexpr static bool is_fix_data_type = true;
  constexpr static bool is_fix_sum_type = false;
  constexpr static bool is_fix_wrapper = false;

  u8x32 content;

private:
  inline Handle( u8x32 content )
    : content( content )
  {}

public:
  static constexpr size_t MAXIMUM_LENGTH = 30;

  explicit inline Handle( const std::string_view str )
    : content {}
  {
    assert( str.size() <= 30 );
    if ( str.size() > 0 )
      memcpy( &content, str.data(), str.size() );
    content[30] = str.size() << 3;
  }

  template<typename T>
  requires std::integral<T> or std::floating_point<T>
  explicit inline Handle( const T x )
    : content {}
  {
    static_assert( sizeof( T ) <= MAXIMUM_LENGTH );
    memcpy( &content, &x, sizeof( T ) );
    content[30] = sizeof( T ) << 3;
  }

  inline static Handle nil()
  {
    const u8x32 zero { 0 };
    return forge( zero );
  }

  inline unsigned char size() const { return content[30] >> 3; }
  inline bool empty() const { return size() == 0; }
  inline size_t local_name() const { throw std::runtime_error( "Attempted to get local name from a literal." ); }
  inline bool is_local() const { return false; }

  inline const char* data() const { return (const char*)&content; }
  inline const std::string_view view() const { return { data(), size() }; }

  static inline Handle<Literal> forge( u8x32 content ) { return { content }; }

  explicit inline operator std::string() const { return std::string( view() ); }

  template<typename T>
  requires std::integral<T> or std::floating_point<T>
  explicit inline operator T() const
  {
    assert( sizeof( T ) == size() );
    return *( (T*)&content );
  }

  template<typename A>
  inline Handle<A> into() const requires std::constructible_from<Handle<A>, Handle<Literal>>
  {
    return Handle<A>( *this );
  }

  template<typename A, typename B, typename... Ts>
  inline auto into() const requires std::constructible_from<Handle<A>, Handle<Literal>>
  {
    return Handle<A>( *this ).template into<B, Ts...>();
  }
};

template<>
struct Handle<Named>
{
  constexpr static bool is_fix_data_type = true;
  constexpr static bool is_fix_sum_type = false;
  constexpr static bool is_fix_wrapper = false;

  u8x32 content;

private:
  inline Handle( const u8x32 content )
    : content( content )
  {}

public:
  inline Handle( const u8x32 hash, uint64_t size )
    : content( hash )
  {
    assert( ( size & 0xffff000000000000 ) == 0 );
    assert( size > Handle<Literal>::MAXIMUM_LENGTH );
    ( *(u64x4*)&content )[3] = size;
  }

  inline Handle( uint64_t local_name, uint64_t size )
    : content()
  {
    ( *(u64x4*)&content )[0] = local_name;
    assert( ( size & 0xffff000000000000 ) == 0 );
    assert( size > Handle<Literal>::MAXIMUM_LENGTH );
    ( *(u64x4*)&content )[3] = size;
    content[30] |= ( 1 << 7 );
  }

  inline u8x32 hash() const
  {
    u64x4 hash = (u64x4)content;
    hash[3] = 0;
    return (u8x32)hash;
  }

  inline size_t size() const { return ( (u64x4)content )[3] & 0xffffffffffff; }
  inline bool empty() const { return size() == 0; }
  inline bool is_local() const { return ( content[30] >> 7 ) & 1; }
  inline uint64_t local_name() const { return ( (u64x4)content )[0]; }

  static inline Handle<Named> forge( u8x32 content ) { return { content }; }

  template<typename A>
  inline Handle<A> into() const requires std::constructible_from<Handle<A>, Handle<Named>>
  {
    return Handle<A>( *this );
  }

  template<typename A, typename B, typename... Ts>
  inline auto into() const requires std::constructible_from<Handle<A>, Handle<Named>>
  {
    return Handle<A>( *this ).template into<B, Ts...>();
  }
};
