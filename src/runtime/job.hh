#pragma once

#include "name.hh"
#include <atomic>

struct Job {
  Name name;
  Name* entry;
  std::atomic<size_t>* pending_jobs;

  Job( Name name,
       Name* entry,
       std::atomic<size_t>* pending_jobs )
    : name( name )
    , entry( entry )
    , pending_jobs( pending_jobs )
  {}

  Job()
    : name()
    , entry()
    , pending_jobs()
  {}
};

