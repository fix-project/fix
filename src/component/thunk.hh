#pragma once

#include <string>
#include <vector>

#include "name.hh"

class Thunk
{
private:
  // Name of the Encode
  Name encode_name_;
  // Path of the Thunk in the output Tree
  std::vector<size_t> path_;

public:
  // Constructor
  Thunk( const Name& encode_name, const std::vector<size_t> path )
    : encode_name_( encode_name )
    , path_( path )
  {}

  const Name& getEncode() const { return encode_name_; }
  const std::vector<size_t>& getPath() const { return path_; }
};
