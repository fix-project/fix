#include "wasienv.hh"

using namespace std;

int WasiEnvironment::path_open( const string & variable_name )
{
  auto & invocation = id_to_inv_.at( invocation_id );

  if ( invocation.isInput( variable_name ) )
  {
    id_to_fd_.insert( pair<uint64_t, FileDescriptor>( next_fd_id_, FileDescriptor( invocation.getInputBlobName( variable_name ), fd_mode::BLOB ) ) );
  } else {
    id_to_fd_.insert( pair<uint64_t, FileDescriptor>( next_fd_id_, FileDescriptor( invocation.getOutputBlobName( variable_name ), fd_mode::ENCODEDBLOB ) ) );
  }

  next_fd_id_++;
  return next_fd_id_ - 1;
}

int WasiEnvironment::fd_read( uint64_t fd_id, uint64_t ofst, uint64_t count )
{
  auto & fd = id_to_fd_.at( fd_id );
  auto & invocation = id_to_inv_.at( invocation_id );

  switch ( fd.mode_ )
  {
    case fd_mode::BLOB :
      memcpy( &invocation.getMem()->data[ ofst ], &runtime_.getBlob( fd.blob_name_ )[ fd.loc_ ], count );
      break;

    case fd_mode::ENCODEDBLOB :
      memcpy( &invocation.getMem()->data[ ofst ], &runtime_.getEncodedBlob( fd.blob_name_ )[ fd.loc_ ], count );
      break;
  }
  
  fd.loc_ += count;
  return count;
}
 
int WasiEnvironment::fd_write( uint64_t fd_id, uint64_t ofst, uint64_t count )
{
  auto & fd = id_to_fd_.at( fd_id );
  auto & invocation = id_to_inv_.at( invocation_id );

  switch ( fd.mode_ )
  {
    case fd_mode::BLOB :
      // TODO: throw exception, case not allowed
      break;

    case fd_mode::ENCODEDBLOB :
      runtime_.getEncodedBlob( fd.blob_name_ ) .append( reinterpret_cast<const char *>( &invocation.getMem()->data[ ofst ] ), count );
      fd.loc_ += count;
      break;
  }

  return count;
}
