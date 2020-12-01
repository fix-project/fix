#include "program.hh"

using namespace std;

Program::Program( string name ) : name_( name ),
                                  fd_( open( name_, O_RDONLY ) ),
                                  fd_wrapper_( FileDescriptor( fd_ ) ),
                                  content_( string( util::filesize( fd_ ), ' ' ) )
{}

Program::load_to_memory() 
{
  fd_wrapper_.read( string_span( content_ ) );
}

void Program::parse( Parser& p ) 
{
  p.integer( num_input_entry_ );
  p.integer( num_output_entry_ );

  size_t input_entry_size = num_input_entry_ * sizeof( InputEntry ); 
  inputs_ = string_span( p.input().substr( 0, input_entry_size ) );
  p.input().remove_prefix( input_entry_size );

  size_t input_entry_size = num_output_entry_ * sizeof( OutputEntry );
  outputs_ = string_span( p.input().substr( 0, output_entry_size ) );
  p.input().remove_prefix( output_entry_size );

  return;
} 

