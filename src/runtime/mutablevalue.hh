#pragma once

#include "name.hh"

struct MutableValueMeta 
{
  // The number of all MTrees/handles/memories that refer the mutable value
  // Is it possible to replace this with shared_ptr?
  unsigned int ref_count_;
  // True if any rw_handle or rw_memory refers the mutable value
  bool rw_reffed;
  bool writable_;
  size_t size_;

  MutableValueMeta()
    : ref_count_( 0 )
    , rw_reffed( false )
    , writable_( true )
    , size_( 0 )
  {}
};

class MutableValue
{
private:
  // Metadata for the memory region is stored at (data - sizeof(MutableValueMeta))
  uint8_t * data;

public:
  ~MutableValue();
};

class MBlob : MutableValue {};
class MTree : MutableValue {};

struct ObjectReference
{
  Name name_;
  bool accessible_;

  ObjectReference()
    : name_()
    , accessible_( true )
  {}
};

using MutableValueReference = MutableValue *;

using RuntimeReference = std::variant<ObjectReference, MutableValueReference>;

struct MTreeEntry
{
  RuntimeReference runtime_reference_;
  bool intended_strictness;
};
