#pragma once

#include <map>

#include "runtime.hh"
#include "invocation.hh"

// ID of the current invocation
inline thread_local uint64_t invocation_id_ = -1;

enum class fd_mode
{
  BLOB,
  ENCODEDBLOB
};

struct WasmFileDescriptor 
{
  std::string blob_name_;
  uint64_t loc_;
  fd_mode mode_;

  WasmFileDescriptor( std::string blob_name, fd_mode mode )
    : blob_name_( blob_name ),
      loc_( 0 ),
      mode_( mode )
  {}

};

class WasiEnvironment 
{
  private:
    // Map from fd id to actual fd
    std::map<uint64_t, WasmFileDescriptor> id_to_fd_;

    // Map from invocation id to actual invocation
    std::map<uint64_t, Invocation> id_to_inv_;

    // System runtime
    Runtime& runtime_; 

    // Next available fd id;
    uint64_t next_fd_id_;

    
    WasiEnvironment( Runtime& runtime ) 
      : id_to_fd_(),
        id_to_inv_(),
        runtime_( runtime ),
        next_fd_id_( 0 )
    {}

  public:
    static WasiEnvironment & getInstance()
    {
      static WasiEnvironment wasi_instance ( Runtime::getInstance() );
      return wasi_instance;
    }

    static int path_open( const std::string & varaible_name ); 
    static int fd_read( uint64_t fd_id, uint64_t ofst, uint64_t count );
    static int fd_write( uint64_t fd_id, uint64_t ofst, uint64_t count );
};



