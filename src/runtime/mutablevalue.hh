#pragma once

#include "name.hh"
#include "tree.hh"

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
  uint8_t* data;

public:
  ~MutableValue();

  void setData( uint8_t* val ) { data = val; }
  uint8_t* getData() { return data; }
};

class MBlob : public MutableValue
{};
class MTree : public MutableValue
{};

struct ObjectReference
{
  Name name_;
  bool accessible_;

  ObjectReference()
    : name_()
    , accessible_( true ) {};

  ObjectReference( TreeEntry entry )
    : name_( entry.first )
    , accessible_( true )
  {
    switch ( entry.second ) {
      case Laziness::Strict:
        accessible_ = true;
        break;
      default:
        accessible_ = false;
    }
  }

  ObjectReference( Name name )
    : name_( name )
    , accessible_( true ) {};
};

using MutableValueReference = MutableValue*;

using RuntimeReference = std::variant<ObjectReference, MutableValueReference>;

struct MTreeEntry
{
  RuntimeReference runtime_reference_;
  bool intended_strictness;
};
