#include "runtimestorage.hh"

#include "invocation.hh"

using namespace std;

WasmFileDescriptor & Invocation::getFd( int fd_id )
{
  return id_to_fd_.at( fd_id );
}

int Invocation::addFd( int fd_id, WasmFileDescriptor fd )
{
  id_to_fd_.try_emplace( fd_id, fd );
  return fd_id;
}

int Invocation::openVariable( const size_t & index )
{
  // If the index is greater than the range of inputs
  if ( index >= input_count_ )
  {
    id_to_fd_.try_emplace( index, WasmFileDescriptor() );
  }
  else 
  {
    // TODO: look for the input in input lists
    id_to_fd_.try_emplace( index, WasmFileDescriptor( getInputName( index ) ) ); 
  }
  return index;
}

uint64_t Invocation::next_invocation_id_ = 0;
