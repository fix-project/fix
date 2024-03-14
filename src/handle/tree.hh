#pragma once
#include <cassert>
#include <concepts> // IWYU pragma: keep
#include <cstddef>
#include <cstdint>

#include "types.hh"

template<typename T>
struct Handle<Tree<T>>
{
  constexpr static bool is_fix_data_type = true;
  constexpr static bool is_fix_sum_type = false;
  constexpr static bool is_fix_wrapper = false;

  u8x32 content;

  using element_type = T;

private:
  Handle( const u8x32 content )
    : content( content )
  {}

public:
  inline Handle( const u8x32 hash, uint64_t size, bool tag = false )
    : content( hash )
  {
    assert( ( size & 0xffff000000000000 ) == 0 );
    ( *(u64x4*)&content )[3] = size;
    content[30] |= ( tag << 6 );
  }

  inline Handle( uint64_t local_name, uint64_t size, bool tag = false )
    : content()
  {
    ( *(u64x4*)&content )[0] = local_name;
    assert( ( size & 0xffff000000000000 ) == 0 );
    ( *(u64x4*)&content )[3] = size;
    content[30] |= ( 1 << 7 );
    content[30] |= ( tag << 6 );
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
  inline bool is_tag() const { return ( content[30] >> 6 ) & 1; }
  inline uint64_t local_name() const { return ( (u64x4)content )[0]; }

  inline Handle<Tree<T>> tag() const
  {
    u8x32 t = content;
    t[30] |= ( 1 << 6 );
    return { t };
  }

  inline Handle<Tree<T>> untag() const
  {
    u8x32 t = content;
    t[30] &= ~( 1 << 6 );
    return { t };
  }

  static inline Handle<Tree<T>> forge( u8x32 content ) { return { content }; }

  explicit inline operator Handle<ObjectTree>() const requires std::same_as<T, Value>
  {
    if ( is_local() ) {
      return { local_name(), size(), is_tag() };
    } else {
      return { hash(), size(), is_tag() };
    }
  }

  explicit inline operator Handle<ExpressionTree>() const requires std::same_as<T, Object> or std::same_as<T, Value>
  {
    if ( is_local() ) {
      return { local_name(), size(), is_tag() };
    } else {
      return { hash(), size(), is_tag() };
    }
  }

  template<typename A>
  inline Handle<A> into() const requires std::constructible_from<Handle<A>, Handle<Tree<T>>>
  {
    return Handle<A>( *this );
  }

  template<typename A>
  inline Handle<A> into( size_t size ) const requires std::constructible_from<Handle<A>, Handle<Tree<T>>, size_t>
  {
    return Handle<A>( *this, size );
  }

  template<typename A, typename B, typename... Ts>
  inline auto into() const requires std::constructible_from<Handle<A>, Handle<Tree<T>>>
  {
    return Handle<A>( *this ).template into<B, Ts...>();
  }
};

template<typename T>
concept FixTreeType = requires( Handle<T> a ) { Handle<ExpressionTree>( a ); };
