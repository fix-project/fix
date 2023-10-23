#include <glog/logging.h>

#include "spans.hh"

void test( void );

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  google::InitGoogleLogging( argv[0] );
  google::SetStderrLogging( google::INFO );
  google::InstallFailureSignalHandler();

  test();
}
