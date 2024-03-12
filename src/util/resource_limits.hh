#pragma once
#include <cstdint>
#include <string>

namespace resource_limits {
extern thread_local uint64_t available_bytes;

class violation : public std::exception
{
  std::string message_;

public:
  violation()
    : violation( "exceeded resource limits" )
  {}

  violation( std::string message )
    : message_( message )
  {}
  virtual const char* what() const noexcept override { return message_.c_str(); }
};
};
