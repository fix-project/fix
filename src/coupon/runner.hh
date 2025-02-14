#pragma once

#include "handle.hh"

class Runner
{
public:
  virtual void init() {};
  virtual Handle<Fix> execute( Handle<Blob> procedure, Handle<Tree> combination ) = 0;
  virtual ~Runner() {}
};

/**
 * For testing and development purposes: a Runner which interprets the first element of a combination as a function
 * pointer and directly jumps to it.
 */
class PointerRunner : public Runner
{

public:
  virtual void init() override {}
  virtual ~PointerRunner() {}

  virtual Handle<Fix> execute( Handle<Blob> handle, Handle<Tree> combination ) override
  {
    auto procedure = handle.try_into<Literal>().value();
    uint64_t addr( procedure );
    auto x = reinterpret_cast<Handle<Fix> ( * )( Handle<Tree> )>( addr );
    return x( combination );
  }
};
