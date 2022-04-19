#pragma once

#include <string>
#include <vector>

#include "name.hh"

class Thunk
{
private:
  // Name of the Encode
  Name encode_name_;

public:
  // Constructor
  Thunk( const Name& encode_name )
    : encode_name_( encode_name )
  {}

  const Name& get_encode() const { return encode_name_; }
};
