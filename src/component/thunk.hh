#pragma once

#include <string>
#include <vector>

#include "handle.hh"

class Thunk
{
private:
  // Handle of the Encode
  Handle encode_name_;

public:
  // Constructor
  Thunk( const Handle encode_name )
    : encode_name_( encode_name )
  {}

  const Handle get_encode() const { return encode_name_; }
};
