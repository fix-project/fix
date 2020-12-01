#include "blob.hh"
#include "util.hh"

Blob::Blob( string name ) : name_( name ),
                            fd_( open( name_, O_RDONLY ) ),
                            fd_wrapper_( FileDescriptor( fd_ ) ),
                            content_( string( util::filesize( fd_ ), ' ' ) )
{}

void Blob::load_to_memory() {
  fd_wrapper_.read( string_span( content_ ) );
} 

void Blob::flush() {
  fd_wrapper_.write( string_span( content_ ) );
}
