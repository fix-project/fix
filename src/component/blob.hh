#pragma once

#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>

struct OptionalFree
{
  bool own { true };

  void operator()( char* ptr ) const
  {
    if ( own ) {
      free( ptr );
    };
  }
};

using unique_char_ptr = std::unique_ptr<char, OptionalFree>;

class Blob
{
private:
  unique_char_ptr data_;
  uint32_t size_;

public:
  Blob( unique_char_ptr&& data, const uint32_t size )
    : data_( std::move( data ) )
    , size_( size )
  {
  }

  Blob( std::string_view str )
    : data_( const_cast<char*>( str.data() ) )
    , size_( str.size() )
  {
    data_.get_deleter().own = false;
    if ( str.size() > std::numeric_limits<uint32_t>::max() ) {
      throw std::out_of_range( "Blob( std::string_view )" );
    }
  }

  Blob( const Blob& ) = delete;
  Blob& operator=( const Blob& ) = delete;

  Blob( Blob&& other ) = default;
  Blob& operator=( Blob&& other ) = default;

  operator std::string_view() const { return { data_.get(), size_ }; }
  const char* data() const { return data_.get(); }
  uint32_t size() const { return size_; }
};

template<typename T>
Blob make_blob( const T& t )
{
  unique_char_ptr t_storage { static_cast<char*>( malloc( sizeof( T ) ) ) };
  if ( not t_storage ) {
    throw std::bad_alloc();
  }
  memcpy( t_storage.get(), &t, sizeof( T ) );
  return { std::move( t_storage ), sizeof( T ) };
}
