#pragma once

#include <string>
#include <sys/stat.h>

#include "file_descriptor.hh"
#include "spans.hh"

namespace util
{
  int filesize( int fd ) 
  {
    struct stat s;
    fstat( fd, &s );
    return ( s.st_size );
  }

  std::string read_file( const std::string& name )
  {
    int fd = open( name, O_RDONLY );
    FileDescriptor fd_wrapper( fd );
    int size = filesize( fd );
    std::string res( size, ' ' );

    fd_wrapper.read( string_span( res ) );
    fd_wrapper.close();

    return res;
  }

  void write_file( const std::string& name, string_span content )
  {
    int fd = open( name, O_WRONLY | O_CREAT );
    FileDescriptor fd_wrapper( fd );
    int size = filesize( fd );

    fd_wrapper.write( content );
    fd_wrapper.close();
    
    return;
  }
}
