#include "wasienv.hh"
#include "runtimestorage.hh"

using namespace std;

namespace wasi
{
  int path_open( const string & variable_name )
  {
    auto & invocation = id_to_inv_.at( invocation_id_ );
    return invocation.openVariable( variable_name );
  }

  int fd_read( uint64_t fd_id, uint64_t ofst, uint64_t count )
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

  int fd_write( uint64_t fd_id, uint64_t ofst, uint64_t count )
  {
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
