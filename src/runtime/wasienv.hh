#pragma once

#include <map>
#include <string>

#include "invocation.hh"

// ID of the current invocation
namespace wasi 
{
  inline thread_local uint64_t invocation_id_ = -1;
  inline std::map<uint64_t, Invocation> id_to_inv_;

  int path_open( uint32_t ofst ); 
  int fd_read( int fd_id, uint32_t ofst, uint32_t count );
  int fd_write( int fd_id, uint32_t ofst, uint32_t count );

  inline int (*Z_envZ_path_openZ_ii)( uint32_t ){ &path_open };
  inline int (*Z_envZ_fd_readZ_iiii)( int, uint32_t, uint32_t ){ &fd_read };
  inline int (*Z_envZ_fd_writeZ_iiii)( int, uint32_t, uint32_t ){ &fd_write };
}
