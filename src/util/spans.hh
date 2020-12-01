#pragma once

#include <cstring>
#include <stdexcept>
#include <string_view>

class string_span : public std::string_view
{
  string_span( std::string_view s )
    : std::string_view( s )
  {}

public:
  using std::string_view::string_view;

  static string_span from_view( std::string_view s ) { return { s }; }

  char* mutable_data() { return const_cast<char*>( data() ); }

  size_t copy( const std::string_view other )
  {
    const size_t amount_to_copy = std::min( size(), other.size() );
    memcpy( mutable_data(), other.data(), amount_to_copy );
    return amount_to_copy;
  }

  string_span substr( const size_t pos, const size_t count ) const
  {
    return from_view( std::string_view::substr( pos, count ) );
  }
};

template<typename T>
class span_view
{
  std::string_view storage_;

  static constexpr auto elem_size_ = sizeof( T );

public:
  span_view( const T* addr, const size_t len )
    : storage_( reinterpret_cast<const char*>( addr ), len * elem_size_ )
  {}

  span_view( const std::string_view s )
    : storage_( s )
  {
    if ( s.size() % elem_size_ ) {
      throw std::runtime_error( "invalid size " + std::to_string( s.size() ) );
    }
  }

  size_t size() const { return storage_.size() / elem_size_; }
  size_t byte_size() const { return storage_.size(); }

  const T* data() const { return reinterpret_cast<const T*>( storage_.data() ); }

  const T& at( const size_t N ) const
  {
    if ( N >= size() ) {
      throw std::out_of_range( "span_view::at: " + std::to_string( N ) + " >= " + std::to_string( size() ) );
    }
    return *( data() + N );
  }

  const T& operator[]( const size_t N ) const { return *( data() + N ); }

  void remove_prefix( const size_t N ) { storage_.remove_prefix( N * elem_size_ ); }

  span_view substr( const size_t pos, const size_t count ) const
  {
    return storage_.substr( pos * elem_size_, count * elem_size_ );
  }

  const T* begin() const { return data(); }
  const T* end() const { return data() + size(); }
};

template<typename T>
class span : public span_view<T>
{
  span( span_view<T> s )
    : span_view<T>( s )
  {}

public:
  using span_view<T>::span_view;

  static span from_view( span_view<T> s ) { return { s }; }

  T* mutable_data() { return const_cast<T*>( span_view<T>::data() ); }

  T& operator[]( const size_t N ) { return *( mutable_data() + N ); }

  T& at( const size_t N )
  {
    if ( N >= span_view<T>::size() ) {
      throw std::out_of_range( "span::at: " + std::to_string( N )
                               + " >= " + std::to_string( span_view<T>::size() ) );
    }
    return *const_cast<T*>( span_view<T>::data() + N );
  }

  T* begin() { return mutable_data(); }
  T* end() { return mutable_data() + span_view<T>::size(); }

  span substr( const size_t pos, const size_t count ) const
  {
    return from_view( span_view<T>::substr( pos, count ) );
  }
};
