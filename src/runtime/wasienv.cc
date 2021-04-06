#include <iostream>

#include "timing_helper.hh"
#include "wasienv.hh"
#include "runtimestorage.hh"

using namespace std;

namespace wasi
{
  int path_open( uint32_t ofst )
  { 
    auto & invocation = id_to_inv_.at( invocation_id_ );

    int fd_id = invocation.openVariable( ofst );
    return fd_id;
  }

  int fd_read( int fd_id, uint32_t ofst, uint32_t count )
  {
    auto & invocation = id_to_inv_.at( invocation_id_ );
    auto & fd = invocation.getFd( fd_id ); 
    
    memcpy( static_cast<void *>( &buf.data[ ofst ] ), &RuntimeStorage::getInstance().getBlob( fd.blob_name_ )[ fd.loc_ ], count );
    
    fd.loc_ += count;
    return count;
  }

  int fd_write( int fd_id, uint32_t ofst, uint32_t count )
  {
    auto & invocation = id_to_inv_.at( invocation_id_ );
    auto & fd = invocation.getFd( fd_id ); 

    if ( (size_t)( fd_id ) < invocation.getInputCount() )
    {
      throw runtime_error ( "No write access." );
    } else {
      fd.buffer.append( reinterpret_cast<const char *>( &buf.data[ ofst ] ), count );
      fd.loc_ += count;
    }

    return count;
  }
}
