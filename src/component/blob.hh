#pragma once

#include <cstdlib>
#include <memory>
#include <string_view>

class Blob
{
private:
  uint8_t* owned_content_;
  size_t size_;

public:
  Blob( uint8_t* data, const size_t size )
    : owned_content_( std::move( data ) )
    , size_( size )
  {
  }

  Blob( Blob&& other )
    : owned_content_( other.owned_content_ )
    , size_( other.size_ )
  {
    if ( this != &other ) {
      other.owned_content_ = nullptr;
    }
  }

  Blob& operator=( Blob&& other )
  {
    owned_content_ = other.owned_content_;
    size_ = other.size_;

    if ( this != &other ) {
      other.owned_content_ = nullptr;
    }
    return *this;
  }

  Blob( const Blob& ) = delete;
  Blob& operator=( const Blob& ) = delete;

  std::string_view content() const { return { reinterpret_cast<char*>( owned_content_ ), size_ }; }
  size_t size() const { return size_; }

  ~Blob()
  {
    if ( owned_content_ ) {
      free( owned_content_ );
    };
  }
};

template<typename T>
Blob make_blob( const T& t )
{
  uint8_t* t_storage = static_cast<uint8_t*>( malloc( sizeof( T ) ) );
  if ( not t_storage ) {
    throw std::bad_alloc();
  }
  memcpy( t_storage, &t, sizeof( T ) );
  return { t_storage, sizeof( T ) };
}
