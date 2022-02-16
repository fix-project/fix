#pragma once

#include <utility>
#include <vector>

#include "name.hh"
#include "spans.hh"

enum class Laziness
{
  Strict,
  Lazy
};

using TreeEntry = std::pair<Name, Laziness>;

class Tree
{
private:
  // Content of the Tree
  std::vector<TreeEntry> content_;

public:
  Tree()
    : content_()
  {}

  Tree( std::vector<TreeEntry>&& content )
    : content_( std::move( content ) )
  {}

  span_view<TreeEntry> content() const { return span_view<TreeEntry>( content_.data(), content_.size() ); }
};
