#pragma once
#include <cassert>
#include <concepts> // IWYU pragma: keep
#include <cstddef>
#include <cstdint>

#include "types.hh"

template<typename T>
struct TreeRef;

template<typename T>
struct Handle<TreeRef<T>>
{
  constexpr static bool is_fix_data_type = true;
  constexpr static bool is_fix_sum_type = false;
  constexpr static bool is_fix_wrapper = true;

  u8x32 content;

  using element_type = T;

private:
  Handle( const u8x32 content )
    : content( content )
  {}

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

public:
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

  static inline Handle<TreeRef<T>> forge( u8x32 content ) { return { content }; }

  inline Handle( const Handle<Tree<T>>& base, size_t size )
    : Handle( base.hash(), size, base.is_tag() )
  {}

  template<typename A>
  requires Handle<A>::is_fix_data_type
  inline Handle( const Handle<A>& base, size_t size ) requires std::convertible_to<Handle<A>, Handle<Tree<T>>>
    : Handle( Handle<Tree<T>>( base ), size )
  {}

  explicit inline operator Handle<ObjectTreeRef>() const requires std::same_as<T, Value>
  {
    if ( is_local() ) {
      return { local_name(), size(), is_tag() };
    } else {
      return { hash(), size(), is_tag() };
    }
  }

  template<typename A>
  inline Handle<A> into() const requires std::constructible_from<Handle<A>, Handle<TreeRef<T>>>
  {
    return Handle<A>( *this );
  }

  template<typename A>
  inline Handle<A> into( size_t size ) const requires std::constructible_from<Handle<A>, Handle<TreeRef<T>>, size_t>
  {
    return Handle<A>( *this, size );
  }

  template<typename A, typename B, typename... Ts>
  inline auto into() const requires std::constructible_from<Handle<A>, Handle<TreeRef<T>>>
  {
    return Handle<A>( *this ).template into<B, Ts...>();
  }
};
