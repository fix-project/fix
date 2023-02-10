#pragma once

struct Entry
{
  Name name;
  std::shared_ptr<std::atomic<int64_t>> pending;

  Entry( Name name, int64_t pending )
    : name( name )
    , pending( new std::atomic<int64_t>( pending ) )
  {}
};
