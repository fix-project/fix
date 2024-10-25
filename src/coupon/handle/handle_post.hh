#pragma once

#include "handle.hh"

template<typename T>
static inline size_t byte_size( Handle<T> handle )
{
  if constexpr ( std::same_as<T, TreeRef> or std::same_as<T, TagRef> or std::same_as<T, BlobRef> ) {
    return 0;
  } else if constexpr ( std::same_as<T, Thunk> ) {
    return 0;
  } else if constexpr ( not Handle<T>::is_fix_sum_type ) {
    return handle.size();
  } else {
    return std::visit( []( const auto x ) { return byte_size( x ); }, handle.get() );
  }
}

template<typename T>
static inline size_t size( Handle<T> handle )
{
  if constexpr ( not Handle<T>::is_fix_sum_type ) {
    return handle.size();
  } else {
    return std::visit( []( const auto x ) { return size( x ); }, handle.get() );
  }
}
