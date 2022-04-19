#pragma once

#include <utility>
#include <vector>

#include "name.hh"
#include "spans.hh"

class Tree
{
private:
  // Content of the Tree
  std::vector<Name> content_;

public:
  Tree()
    : content_()
  {}

  Tree( std::vector<Name>&& content )
    : content_( std::move( content ) )
  {}

  span_view<Name> content() const { return span_view<Name>( content_.data(), content_.size() ); }
};
