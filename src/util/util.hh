#pragma once

#include <fcntl.h>
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

  std::string read_file( const std::string & name )
  {
    int fd = open( name.c_str(), O_RDONLY );
    FileDescriptor fd_wrapper( fd );
    int size = filesize( fd );
    std::string res( size, ' ' );

    fd_wrapper.read( string_span::from_view( res ) );
    fd_wrapper.close();

    return res;
  }

  void write_file( const std::string & name, string_span content )
  {
    int fd = open( name.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR );
    FileDescriptor fd_wrapper( fd );

    fd_wrapper.write( content );
    fd_wrapper.close();
    
    return;
  }
}
