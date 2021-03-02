#include <iostream>

#include "timing_helper.hh"
#include "wasienv.hh"
#include "runtimestorage.hh"

using namespace std;

namespace wasi
{
  int path_open( uint32_t ofst )
  { 
    RecordScopeTimer<Timer::Category::Nonblock> record_timer { _path_open };
    auto & invocation = id_to_inv_.at( invocation_id_ );
    string variable_name = string( reinterpret_cast<char *>( &invocation.getMem()->data[ ofst ] ) );
    int fd_id = invocation.openVariable( variable_name );
    return fd_id;
  }

  int fd_read( int fd_id, uint32_t ofst, uint32_t count )
  {
    auto & invocation = id_to_inv_.at( invocation_id_ );
    auto & fd = invocation.getFd( fd_id ); 

    switch ( fd.mode_ )
    {
      case fd_mode::BLOB :
        memcpy( &invocation.getMem()->data[ ofst ], &RuntimeStorage::getInstance().getBlob( fd.blob_name_ )[ fd.loc_ ], count );
        break;

      case fd_mode::ENCODEDBLOB :
        memcpy( &invocation.getMem()->data[ ofst ], &RuntimeStorage::getInstance().getEncodedBlob( fd.blob_name_ )[ fd.loc_ ], count );
        break;
    }

    fd.loc_ += count;
    return count;
  }

  int fd_write( int fd_id, uint32_t ofst, uint32_t count )
  {
    RecordScopeTimer<Timer::Category::Nonblock> record_timer { _fd_write };
    auto & invocation = id_to_inv_.at( invocation_id_ );
    auto & fd = invocation.getFd( fd_id ); 

    switch ( fd.mode_ )
    {
      case fd_mode::BLOB :
        // TODO: throw exception, case not allowed
        break;

      case fd_mode::ENCODEDBLOB :
        RuntimeStorage::getInstance().getEncodedBlob( fd.blob_name_ ).append( reinterpret_cast<const char *>( &invocation.getMem()->data[ ofst ] ), count );
        fd.loc_ += count;
        break;
    }

    return count;
  }
}
