#pragma once

#include <vector>

#include "name.hh"

enum class Laziness
{
  Strict,
  Lazy
};

struct TreeEntry
{
  Name name_;
  Laziness laziness_;
};

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
    : content_( content )
  {}
};
