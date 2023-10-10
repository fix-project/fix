#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "base64.hh"
#include "tester-utils.hh"

#define TRUSTED_COMPILE_ENCODE Handle( base64::decode( COMPILE_ENCODE ) )

using namespace std;
using path = filesystem::path;

void program_body( span_view<char*> args )
{
  path repo( args[1] );
  string name( args[2] );
  Handle encode( base64::decode( name ) );

  auto& runtime = RuntimeStorage::get_instance();
  runtime.deserialize_from_dir( repo );
  ios::sync_with_stdio( false );

  Handle result = runtime.eval_thunk( runtime.add_thunk( Thunk( encode ) ) );

  filesystem::path out_repo = filesystem::temp_directory_path()
                              / filesystem::path( "fix-repo-" + to_string( getpid() ) + "." + to_string( rand() ) );
  filesystem::create_directories( out_repo );

  string filename = runtime.serialize_objects( result, out_repo );
  cout << ( out_repo / filename ).string();
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " path/to/repo name_of_encode";
}

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  try {
    if ( argc < 3 ) {
      usage_message( argv[0] );
      return EXIT_FAILURE;
    }

    span_view<char*> args = { argv, static_cast<size_t>( argc ) };
    program_body( args );
  } catch ( const exception& e ) {
    cerr << argv[0] << ": " << e.what() << "\n\n";
    usage_message( argv[0] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
