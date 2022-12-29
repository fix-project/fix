#include <iostream>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "exception.hh"
#include "mmap.hh"

using namespace std;

MMap_Region::MMap_Region( char* const addr,
                          const size_t length,
                          const int prot,
                          const int flags,
                          const int fd,
                          const off_t offset )
  : addr_( static_cast<char*>( mmap( addr, length, prot, flags, fd, offset ) ) )
  , length_( length )
{
  if ( addr_ == MAP_FAILED ) {
    throw unix_error( "mmap" );
  }
}

MMap_Region::~MMap_Region()
{
  if ( addr_ ) {
    try {
      CheckSystemCall( "munmap", munmap( addr_, length_ ) );
    } catch ( const exception& e ) {
      cerr << "Exception destructing MMap_Region: " << e.what() << endl;
    }
  }
}

MMap_Region::MMap_Region( MMap_Region&& other ) noexcept
  : addr_( other.addr_ )
  , length_( other.length_ )
{
  other.addr_ = nullptr;
  other.length_ = 0;
}

MMap_Region& MMap_Region::operator=( MMap_Region&& other ) noexcept
{
  addr_ = other.addr_;
  length_ = other.length_;

  other.addr_ = nullptr;
  other.length_ = 0;

  return *this;
}

ReadOnlyFile::ReadOnlyFile( FileDescriptor&& fd )
  : MMap_Region( nullptr, fd.size(), PROT_READ, MAP_SHARED, fd.fd_num() )
  , fd_( move( fd ) )
{}

ReadOnlyFile::ReadOnlyFile( const string& filename )
  : ReadOnlyFile(
    FileDescriptor { CheckSystemCall( "open( \"" + filename + "\" )", open( filename.c_str(), O_RDONLY ) ) } )
{}

ReadWriteFile::ReadWriteFile( FileDescriptor&& fd )
  : MMap_Region( nullptr, fd.size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd.fd_num() )
  , fd_( move( fd ) )
{}
