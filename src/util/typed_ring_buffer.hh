#pragma once

#include "exception.hh"
#include "ring_buffer.hh"
#include "spans.hh"

#include <algorithm>
#include <iostream>

template<typename T>
class TypedRingStorage : public RingStorage
{
  static constexpr auto elem_size_ = sizeof( T );

protected:
  span<T> mutable_storage( const size_t index ) { return { RingStorage::storage( index * elem_size_ ) }; }
  span_view<T> storage( const size_t index ) const { return { RingStorage::storage( index * elem_size_ ) }; }

public:
  explicit TypedRingStorage( const size_t capacity )
    : RingStorage( capacity * elem_size_ )
  {}

  size_t capacity() const { return RingStorage::capacity() / elem_size_; }
};

template<typename T>
class TypedRingBuffer : public TypedRingStorage<T>
{
  size_t num_pushed_ = 0, num_popped_ = 0;

  size_t next_index_to_write() const { return num_pushed_ % capacity(); }
  size_t next_index_to_read() const { return num_popped_ % capacity(); }

public:
  using TypedRingStorage<T>::TypedRingStorage;
  using TypedRingStorage<T>::capacity;

  size_t num_pushed() const { return num_pushed_; }
  size_t num_popped() const { return num_popped_; }
  size_t num_stored() const { return num_pushed_ - num_popped_; }

  span<T> writable_region()
  {
    return TypedRingStorage<T>::mutable_storage( next_index_to_write() ).substr( 0, capacity() - num_stored() );
  }

  span_view<T> writable_region() const
  {
    return TypedRingStorage<T>::storage( next_index_to_write() ).substr( 0, capacity() - num_stored() );
  }

  void push( const size_t num_elems )
  {
    if ( num_elems > writable_region().size() ) {
      throw std::runtime_error( "TypedRingBuffer::push exceeded size of writable region" );
    }

    num_pushed_ += num_elems;
  }

  span_view<T> readable_region() const
  {
    return TypedRingStorage<T>::storage( next_index_to_read() ).substr( 0, num_stored() );
  }

  void pop( const size_t num_elems )
  {
    if ( num_elems > readable_region().size() ) {
      throw std::runtime_error( "TypedRingBuffer::pop exceeded size of readable region" );
    }

    num_popped_ += num_elems;
  }
};

template<typename T>
class EndlessBuffer : TypedRingStorage<T>
{
  size_t num_popped_ = 0;

  void check_bounds( const size_t pos, const size_t count ) const
  {
    if ( pos < range_begin() ) {
      std::cerr << std::string( "check_bounds: " + std::to_string( pos ) + " < " + std::to_string( range_begin() ) )
                << std::endl;
      abort();
    }

    if ( pos + count > range_end() ) {
      std::cerr << std::string( "check_bounds: " + std::to_string( pos ) + " + " + std::to_string( count ) + " > "
                                + std::to_string( range_end() ) )
                << std::endl;
      abort();
    }
  }

  size_t next_index_to_read() const { return num_popped_ % TypedRingStorage<T>::capacity(); }

  span_view<T> readable_region() const { return TypedRingStorage<T>::storage( next_index_to_read() ); }
  span<T> readable_region() { return TypedRingStorage<T>::mutable_storage( next_index_to_read() ); }

public:
  using TypedRingStorage<T>::TypedRingStorage;

  void pop( const size_t num_elems )
  {
    span<T> region_to_erase { readable_region().substr( 0, num_elems ) };
    std::fill( region_to_erase.begin(), region_to_erase.end(), T {} );
    num_popped_ += num_elems;
  }

  size_t range_begin() const { return num_popped_; }
  size_t range_end() const { return range_begin() + TypedRingStorage<T>::capacity(); }

  span<T> region( const size_t pos, const size_t count )
  {
    check_bounds( pos, count );
    return readable_region().substr( pos - range_begin(), count );
  }

  span_view<T> region( const size_t pos, const size_t count ) const
  {
    check_bounds( pos, count );
    return readable_region().substr( pos - range_begin(), count );
  }

  T& at( const size_t pos ) { return region( pos, 1 ).at( 0 ); }
  const T& at( const size_t pos ) const { return region( pos, 1 ).at( 0 ); }

  const T& operator[]( const size_t pos ) const { return readable_region().substr( pos - range_begin(), 1 )[0]; }
  T& operator[]( const size_t pos ) { return readable_region().substr( pos - range_begin(), 1 )[0]; }
};

template<typename T>
class SafeEndlessBuffer : public EndlessBuffer<T>
{
public:
  using EndlessBuffer<T>::EndlessBuffer;

  T safe_get( const size_t pos ) const
  {
    if ( pos < EndlessBuffer<T>::range_begin() or pos >= EndlessBuffer<T>::range_end() ) {
      return {};
    }

    return EndlessBuffer<T>::region( pos, 1 ).at( 0 );
  }

  void safe_set( const size_t pos, const T& val )
  {
    if ( pos < EndlessBuffer<T>::range_begin() or pos >= EndlessBuffer<T>::range_end() ) {
      return;
    }

    EndlessBuffer<T>::region( pos, 1 ).at( 0 ) = val;
  }
};
