#pragma once

#include <fstream>
#include <sstream>
#include <string>

#include "file_descriptor.hh"
#include "spans.hh"

namespace util {
inline std::string read_file( const std::string& name )
{
  std::ifstream inFile( name );
  std::stringstream inString;

  inString << inFile.rdbuf();
  return inString.str();
}
}
