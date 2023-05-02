#pragma once

#include <atomic>

#include "name.hh"

#define NONE 0x00
#define APPLY 0x01
#define FORCE 0x02
#define EVAL 0x03
#define LINK 0x04
#define FILL 0x05

struct Job
{
  Name name;
  Name hash;

  Job( Name name, Name hash )
    : name( name )
    , hash( hash )
  {}

  Job()
    : name()
    , hash()
  {}
};
