#pragma once
#include <cassert>
#include <concepts> // IWYU pragma: keep
#include <cstddef>
#include <cstdint>

#include "types.hh"

template<typename T>
struct Handle<Tree<T>>
{
  u8x32 content;

private:
  Handle( const unsigned char __attribute__( ( vector_size( 32 ) ) ) content )
    : content( content )
  {}

public:
  inline Handle( u8x32 hash, uint64_t size )
    : content( hash )
  {
    assert( ( size & 0xffff000000000000 ) == 0 );
    ( *(u64x4*)&content )[3] = size;
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

  explicit inline operator Handle<ValueTree>() const requires std::same_as<T, Object>
  {
    return Handle<ValueTree>::forge( content );
  }

  explicit inline operator Handle<ExpressionTree>() const requires std::same_as<T, Value> or std::same_as<T, Object>
  {
    return Handle<ExpressionTree>::forge( content );
  }

  explicit inline operator Handle<FixTree>() const
    requires std::same_as<T, Value> or std::same_as<T, Object> or std::same_as<T, Expression>
  {
    return Handle<FixTree>::forge( content );
  }

  template<typename S>
  inline Handle<S> into() const requires std::constructible_from<Handle<S>, Handle<Tree<T>>>
  {
    return Handle<S>( *this );
  }
};
