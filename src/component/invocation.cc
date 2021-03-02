#include "runtimestorage.hh"

#include "invocation.hh"

using namespace std;

bool Invocation::isInput( const string & variable_name )
{
  return RuntimeStorage::getInstance().getEncode( encode_name_ ).input_to_blob_.contains( variable_name );
}

bool Invocation::isOutput( const string & variable_name )
{
  return RuntimeStorage::getInstance().getEncode( encode_name_ ).output_to_blob_.contains( variable_name );
}

string Invocation::getInputBlobName( const string & variable_name )
{
  return RuntimeStorage::getInstance().getEncode( encode_name_ ).input_to_blob_.at( variable_name );
}

string Invocation::getOutputBlobName( const string & variable_name )
{
  return RuntimeStorage::getInstance().getEncode( encode_name_ ).output_to_blob_.at( variable_name );
}

WasmFileDescriptor & Invocation::getFd( int fd_id )
{
  return id_to_fd_.at( fd_id );
}

int Invocation::addFd( WasmFileDescriptor fd )
{
  id_to_fd_.push_back( fd );
  return id_to_fd_.size() - 1;
}

int Invocation::openVariable( const string & variable_name )
{
  if ( isInput( variable_name ) )
  {
    id_to_fd_.push_back( WasmFileDescriptor( getInputBlobName( variable_name ), fd_mode::BLOB ) );
  } 
  else if ( isOutput( variable_name ) ) {
    id_to_fd_.push_back( WasmFileDescriptor( getOutputBlobName( variable_name ), fd_mode::ENCODEDBLOB ) );
    RuntimeStorage::getInstance().getEncodedBlob( getOutputBlobName( variable_name ) ) = "";
  } else {
    throw out_of_range ( variable_name + " not an input or output." );
  }

  return id_to_fd_.size() - 1;
}

uint64_t Invocation::next_invocation_id_ = 0;
