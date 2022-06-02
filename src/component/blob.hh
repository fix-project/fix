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
  std::shared_ptr<char> raw_content_;
  size_t size_;

public:
  // Construct from name and content
  Blob( std::string&& content )
    : content_( std::move( content ) )
    , raw_content_( { const_cast<char*>( content_.data() ), []( char* ) {} } )
    , size_( content_.size() )
  {
  }

  Blob( char* data, size_t size )
    : content_()
    , raw_content_( { data, free } )
    , size_( size )
  {
  }

  std::string_view content() const { return std::string_view( raw_content_.get(), size_ ); }
  size_t size() const { return size_; }
};
