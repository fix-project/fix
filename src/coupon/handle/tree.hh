#pragma once
#include <cassert>
#include <concepts> // IWYU pragma: keep
#include <cstddef>
#include <cstdint>

#include "types.hh"

template<>
struct Handle<Tree>
{
  constexpr static bool is_fix_data_type = true;
  constexpr static bool is_fix_sum_type = false;
  constexpr static bool is_fix_wrapper = false;

  u8x32 content;

private:
  Handle( const u8x32 content )
    : content( content )
  {}

public:
  inline Handle( const u8x32 hash, uint64_t size )
    : content( hash )
  {
    assert( ( size & 0xffff000000000000 ) == 0 );
    ( *(u64x4*)&content )[3] = size;
  }

  inline Handle( uint64_t local_name, uint64_t size )
    : content()
  {
    ( *(u64x4*)&content )[0] = local_name;
    assert( ( size & 0xffff000000000000 ) == 0 );
    ( *(u64x4*)&content )[3] = size;
    content[30] |= ( 1 << 7 );
  }

  inline static Handle nil()
  {
    const u8x32 zero { 0 };
    return forge( zero );
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

  static inline Handle<Tree> forge( u8x32 content ) { return { content }; }

  template<typename A>
  inline Handle<A> into() const requires std::constructible_from<Handle<A>, Handle<Tree>>
  {
    return Handle<A>( *this );
  }

  template<typename A>
  inline Handle<A> into( size_t size ) const requires std::constructible_from<Handle<A>, Handle<Tree>, size_t>
  {
    return Handle<A>( *this, size );
  }

  template<typename A, typename B, typename... Ts>
  inline auto into() const requires std::constructible_from<Handle<A>, Handle<Tree>>
  {
    return Handle<A>( *this ).template into<B, Ts...>();
  }
};

template<>
struct Handle<Tag>
{
  constexpr static bool is_fix_data_type = true;
  constexpr static bool is_fix_sum_type = false;
  constexpr static bool is_fix_wrapper = false;

  u8x32 content;

private:
  Handle( const u8x32 content )
    : content( content )
  {}

public:
  inline Handle( const u8x32 hash, uint64_t size )
    : content( hash )
  {
    assert( ( size & 0xffff000000000000 ) == 0 );
    ( *(u64x4*)&content )[3] = size;
  }

  inline Handle( uint64_t local_name, uint64_t size )
    : content()
  {
    ( *(u64x4*)&content )[0] = local_name;
    assert( ( size & 0xffff000000000000 ) == 0 );
    ( *(u64x4*)&content )[3] = size;
    content[30] |= ( 1 << 7 );
  }

  inline static Handle nil()
  {
    const u8x32 zero { 0 };
    return forge( zero );
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

  static inline Handle<Tag> forge( u8x32 content ) { return { content }; }

  template<typename A>
  inline Handle<A> into() const requires std::constructible_from<Handle<A>, Handle<Tree>>
  {
    return Handle<A>( *this );
  }

  template<typename A>
  inline Handle<A> into( size_t size ) const requires std::constructible_from<Handle<A>, Handle<Tag>, size_t>
  {
    return Handle<A>( *this, size );
  }

  template<typename A, typename B, typename... Ts>
  inline auto into() const requires std::constructible_from<Handle<A>, Handle<Tree>>
  {
    return Handle<A>( *this ).template into<B, Ts...>();
  }
};
