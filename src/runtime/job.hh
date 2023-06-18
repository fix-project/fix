#pragma once

#include <atomic>

#include "handle.hh"

#define NONE 0x00
#define APPLY 0x01
#define MAKE_SHALLOW 0x02
#define EVAL 0x03
#define LINK 0x04
#define FILL 0x05

struct Job
{
  Handle name;
  Handle hash;

  Job( Handle name, Handle hash )
    : name( name )
    , hash( hash )
  {}

  Job()
    : name()
    , hash()
  {}
};
