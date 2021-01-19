#pragma once

#include <map>
#include <string>

#include "invocation.hh"

// ID of the current invocation
namespace wasi 
{
  inline thread_local uint64_t invocation_id_ = -1;
  static std::map<uint64_t, Invocation> id_to_inv_;

  int path_open( const std::string & varaible_name ); 
  int fd_read( uint64_t fd_id, uint64_t ofst, uint64_t count );
  int fd_write( uint64_t fd_id, uint64_t ofst, uint64_t count );
}
