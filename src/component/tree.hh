#pragma once

#include <utility>
#include <vector>

#include "name.hh"
#include "spans.hh"

static_assert( sizeof( __m256i ) == sizeof( Name ) );

class Tree
{
private:
  // Content of the Tree
  Name* owned_content_;
  size_t size_;

public:
  Tree( Name* content, size_t size )
    : owned_content_( content )
    , size_( size )
  {
  }

  Tree( __m256i* content, size_t size )
    : owned_content_( reinterpret_cast<Name*>( content ) )
    , size_( size )
  {
  }

  Tree( Tree&& other )
    : owned_content_( other.owned_content_ )
    , size_( other.size_ )
  {
    if ( this != &other ) {
      other.owned_content_ = nullptr;
    }
  }

  Tree& operator=( Tree&& other )
  {
    owned_content_ = other.owned_content_;
    size_ = other.size_;

    if ( this != &other ) {
      other.owned_content_ = nullptr;
    }
    return *this;
  }

  Tree( const Tree& ) = delete;
  Tree& operator=( const Tree& ) = delete;

  span_view<Name> content() const { return { owned_content_, size_ }; }
  size_t size() const { return size_; }

  ~Tree()
  {
    if ( owned_content_ ) {
      free( owned_content_ );
    };
  }

  static Tree make_tree( const std::vector<Name>& other )
  {
    Name* storage = static_cast<Name*>( aligned_alloc( alignof( Name ), sizeof( Name ) * other.size() ) );
    if ( not storage ) {
      throw std::bad_alloc();
    }

    memcpy( storage, other.data(), sizeof( Name ) * other.size() );
    return { storage, other.size() };
  }
};
