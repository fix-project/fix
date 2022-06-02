#pragma once

#include <utility>
#include <vector>

#include "name.hh"
#include "spans.hh"

class Tree
{
private:
  // Content of the Tree
  std::vector<Name> vector_content_;
  std::shared_ptr<Name> raw_content_;
  size_t size_;

public:
  Tree()
    : vector_content_()
    , raw_content_()
    , size_()
  {
  }

  Tree( std::vector<Name>&& content )
    : vector_content_( std::move( content ) )
    , raw_content_( { vector_content_.data(), []( Name* ) {} } )
    , size_( vector_content_.size() )
  {
  }

  Tree( Name* content, size_t size )
    : vector_content_()
    , raw_content_( { content, free } )
    , size_( size )
  {
  }

  span_view<Name> content() const { return span_view<Name>( raw_content_.get(), size_ ); }
};
