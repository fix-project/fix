#pragma once

#include <atomic>

#include "handle.hh"

struct Entry
{
  Handle name;
  std::shared_ptr<std::atomic<int64_t>> pending;

  Entry( Handle name, int64_t pending )
    : name( name )
    , pending( new std::atomic<int64_t>( pending ) )
  {}

  Entry( Handle name )
    : name( name )
    , pending()
  {}
};
