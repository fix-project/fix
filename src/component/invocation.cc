#include "invocation.hh"

using namespace std;

bool Invocation::isInput( const string & variable_name )
{
  auto it = input_to_blob_.find( variable_name );
  return it != input_to_blob_.end();
}

bool Invocation::isOutput( const string & variable_name )
{
  auto it = output_to_blob_.find( variable_name );
  return it != output_to_blob_.end();
}

string Invocation::getInputBlobName( const string & variable_name )
{
  return input_to_blob_.at( variable_name );
}

string Invocation::getOutputBlobName( const string & variable_name )
{
  return output_to_blob_.at( variable_name );
}

WasmFileDescriptor & Invocation::getFd( int fd_id )
{
  return id_to_fd_.at( fd_id );
}

int Invocation::addFd( WasmFileDescriptor fd )
{
  id_to_fd_.insert( pair<int, WasmFileDescriptor>( next_fd_id_, fd ) );
  next_fd_id_++;
  return next_fd_id_ - 1;
}

int Invocation::openVariable( const string & variable_name )
{
  if ( isInput( variable_name ) )
  {
    id_to_fd_.insert( pair<int, WasmFileDescriptor>( next_fd_id_, WasmFileDescriptor( getInputBlobName( variable_name ), fd_mode::BLOB ) ) );
  } 
  else if ( isOutput( variable_name ) ) {
    id_to_fd_.insert( pair<int, WasmFileDescriptor>( next_fd_id_, WasmFileDescriptor( getOutputBlobName( variable_name ), fd_mode::ENCODEDBLOB ) ) );
  } else {
    throw out_of_range ( variable_name + " not an input or output." );
  }

  next_fd_id_++;
  return next_fd_id_ - 1;
}

uint64_t Invocation::next_invocation_id_ = 0;
