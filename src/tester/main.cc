#include <glog/logging.h>

#include <span>

extern int max_args;
extern int min_args;
extern void usage_message( const char* argv0 );
extern void program_body( std::span<char*> args );

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  google::InitGoogleLogging( argv[0] );
  google::SetStderrLogging( google::INFO );
  google::InstallFailureSignalHandler();

  if ( ( min_args >= 0 and argc < min_args ) or ( max_args >= 0 and argc > max_args ) ) {
    usage_message( argv[0] );
    return EXIT_FAILURE;
  }

  std::span<char*> args = { argv, static_cast<size_t>( argc ) };
  program_body( args );

  return EXIT_SUCCESS;
}
