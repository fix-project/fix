#include <sys/mman.h>
#include <unistd.h>

#include "exception.hh"
#include "file_descriptor.hh"
#include "ring_buffer.hh"

using namespace std;

RingStorage::RingStorage( const size_t capacity )
  : fd_( [&] {
    if ( capacity % sysconf( _SC_PAGESIZE ) ) {
      throw runtime_error( "RingBuffer capacity must be multiple of page size ("
                           + to_string( sysconf( _SC_PAGESIZE ) ) + "), which " + to_string( capacity )
                           + " isn't" );
    }
    FileDescriptor fd { CheckSystemCall( "memfd_create", memfd_create( "RingBuffer", 0 ) ) };
    CheckSystemCall( "ftruncate", ftruncate( fd.fd_num(), capacity ) );
    return fd;
  }() )
  , virtual_address_space_( nullptr, 2 * capacity, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, -1 )
  , first_mapping_( virtual_address_space_.addr(),
                    capacity,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_FIXED,
                    fd_.fd_num() )
  , second_mapping_( virtual_address_space_.addr() + capacity,
                     capacity,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_FIXED,
                     fd_.fd_num() )
{}

size_t RingBuffer::next_index_to_write() const
{
  return bytes_pushed_ % capacity();
}

size_t RingBuffer::next_index_to_read() const
{
  return bytes_popped_ % capacity();
}

std::string_view RingBuffer::writable_region() const
{
  return storage( next_index_to_write() ).substr( 0, capacity() - bytes_stored() );
}

string_span RingBuffer::writable_region()
{
  return mutable_storage( next_index_to_write() ).substr( 0, capacity() - bytes_stored() );
}

void RingBuffer::push( const size_t num_bytes )
{
  if ( num_bytes > writable_region().length() ) {
    throw runtime_error( "RingBuffer::push exceeded size of writable region" );
  }

  bytes_pushed_ += num_bytes;
}

std::string_view RingBuffer::readable_region() const
{
  return storage( next_index_to_read() ).substr( 0, bytes_stored() );
}

void RingBuffer::pop( const size_t num_bytes )
{
  if ( num_bytes > readable_region().length() ) {
    throw runtime_error( "RingBuffer::pop exceeded size of readable region" );
  }

  bytes_popped_ += num_bytes;
}

size_t RingBuffer::push_from_const_str( const string_view str )
{
  const size_t bytes_written = writable_region().copy( str );
  push( bytes_written );
  return bytes_written;
}
