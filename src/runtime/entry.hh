#pragma once

#include <atomic>

#include "name.hh"

struct Entry
{
  Name name;
  std::shared_ptr<std::atomic<int64_t>> pending;

  Entry( Name name, int64_t pending )
    : name( name )
    , pending( new std::atomic<int64_t>( pending ) )
  {}

  Entry( Name name )
    : name( name )
    , pending()
  {}
};
