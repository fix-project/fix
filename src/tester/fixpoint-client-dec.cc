#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sys/ioctl.h>
#include <unistd.h>

#include "handle.hh"
#include "object.hh"
#include "runtimes.hh"
#include "tester-utils.hh"
#include "types.hh"

using namespace std;

Handle<Thunk> compile( IRuntime& rt, std::string_view file )
{
  auto wasm = rt.create( std::make_shared<OwnedBlob>( file ) );
  auto compile = rt.labeled( "compile-encode" ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();
  auto application = OwnedMutTree::allocate( 3 );
  application[0] = make_limits( rt, 1024 * 1024 * 1024, 1024 * 1024, 1, false );
  application[1] = Handle<Strict>( Handle<Identification>( compile ) );
  application[2] = wasm;
  auto handle = rt.create( make_shared<OwnedTree>( std::move( application ) ) ).unwrap<ExpressionTree>();
  return Handle<Application>( handle::upcast( handle ) );
}

int main( int argc, char* argv[] )
{
  if ( argc != 3 ) {
    cerr << "Usage: " << argv[0] << " num-of-call address:port" << endl;
    exit( 1 );
  }

  int num_call = stoi( string( argv[1] ) );

  auto client_rt = [&] {
    std::shared_ptr<Client> client;
    string addr( argv[2] );
    if ( addr.find( ':' ) == string::npos ) {
      throw runtime_error( "invalid argument " + addr );
    }
    Address address( addr.substr( 0, addr.find( ':' ) ), stoi( addr.substr( addr.find( ':' ) + 1 ) ) );
    client = Client::init( address );
    std::shared_ptr<FrontendRT> frontend = dynamic_pointer_cast<FrontendRT>( client );
    return make_pair( frontend, std::reference_wrapper( client->get_rt() ) );
  };

  auto [client, rt] = client_rt();

  auto dec_runnable = client->execute( Handle<Eval>( compile( rt, "build/testing/wasm-examples/dec.wasm" ) ) );

  auto first_run = OwnedMutTree::allocate( 3 );
  first_run[0] = make_limits( rt, 1024 * 1024 * 1024, 1024, 1 );
  first_run[1] = dec_runnable;
  first_run[2] = Handle<Literal>( (int)0 );
  auto first_run_goal = Handle<Application>(
    Handle<ExpressionTree>( rt.create( make_shared<OwnedTree>( std::move( first_run ) ) ).unwrap<ValueTree>() ) );
  client->execute( Handle<Eval>( first_run_goal ) );

  auto invocation = OwnedMutTree::allocate( 3 );
  invocation[0] = make_limits( rt, 1024 * 1024 * 1024, 1024, 1 );
  invocation[1] = dec_runnable;
  invocation[2] = Handle<Literal>( (int)( num_call - 1 ) );

  auto goal = Handle<Application>(
    Handle<ExpressionTree>( rt.create( make_shared<OwnedTree>( std::move( invocation ) ) ).unwrap<ValueTree>() ) );

  cerr << "Executing." << endl;
  struct timespec before, after;
  struct timespec before_real, after_real;
  if ( clock_gettime( CLOCK_REALTIME, &before_real ) ) {
    perror( "clock_gettime" );
    exit( 1 );
  }
  if ( clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &before ) ) {
    perror( "clock_gettime" );
    exit( 1 );
  }

  client->execute( Handle<Eval>( goal ) );

  if ( clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &after ) ) {
    perror( "clock_gettime" );
    exit( 1 );
  }
  if ( clock_gettime( CLOCK_REALTIME, &after_real ) ) {
    perror( "clock_gettime" );
    exit( 1 );
  }

  double delta_real
    = after_real.tv_sec - before_real.tv_sec + (double)( after_real.tv_nsec - before_real.tv_nsec ) * 1e-9;

  cerr << "Real Time: " << delta_real << " seconds" << endl;

  return 0;
}
