#pragma once

#include "couponcollector.hh"
#include "runtime.hh"

class Tagger
{
  DeterministicTagRuntime& rt_;

public:
  Tagger( DeterministicTagRuntime& rt )
    : rt_( rt )
  {}

  std::optional<EquivalenceTag> exchange_tag( Handle<Tag>, EquivalenceTag );
  std::optional<EvalTag> exchange_tag( Handle<Tag>, EvalTag );
  std::optional<ReduceTag> exchange_tag( Handle<Tag>, ReduceTag );
};
