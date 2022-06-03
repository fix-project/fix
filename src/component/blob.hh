#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "sha256.hh"

class Blob
{
private:
  // Content of the blob
  const std::string content_;
  char* raw_content_;
  size_t size_;
  bool free_;

public:
  // Construct from name and content
  Blob( std::string&& content )
    : content_( std::move( content ) )
    , raw_content_( const_cast<char*>( content_.data() ) )
    , size_( content_.size() )
    , free_( false )
  {
  }

  Blob( char* data, size_t size )
    : content_()
    , raw_content_( data )
    , size_( size )
    , free_( true )
  {
  }

  Blob(const Blob&) = delete;
  Blob& operator=(const Blob&) = delete;
  Blob(Blob&&) = default;

  ~Blob() {
    if ( free_ ) {
      free( raw_content_ );
    }
  }

  std::string_view content() const { return std::string_view( raw_content_, size_ ); }
  size_t size() const { return size_; }
};
