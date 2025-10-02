#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sys/ioctl.h>
#include <unistd.h>

#include "handle.hh"
#include "handle_post.hh"
#include "object.hh"
#include "runtimes.hh"
#include "tester-utils.hh"
#include "types.hh"

using namespace std;

int min_args = 0;
int max_args = -1;

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
    cerr << "Usage: " << argv[0] << " input-label address:port" << endl;
    exit( 1 );
  }

  auto input_label = std::string( argv[1] );

  string addr( argv[2] );
  if ( addr.find( ':' ) == string::npos ) {
    throw runtime_error( "invalid argument " + addr );
  }
  Address address( addr.substr( 0, addr.find( ':' ) ), stoi( addr.substr( addr.find( ':' ) + 1 ) ) );
  auto client = Client::init( address );
  auto& rt = client->get_rt();

  auto add = client->execute( Handle<Eval>( compile( rt, "build/testing/wasm-examples/add-simple.wasm" ) ) );
  cout << add << endl;

  auto input_tree_handle
    = handle::data( rt.labeled( input_label ) )
        .value()
        .visit<Handle<ValueTree>>( overload {
          [&]( Handle<ValueTree> tree ) { return tree; },
          [&]( auto ) -> Handle<ValueTree> { throw runtime_error( "haystack label was not a blob" ); },
        } );

  auto input_tree = rt.get( input_tree_handle ).value();

  auto add_link_tree = OwnedMutTree::allocate( 4 );
  add_link_tree[0] = make_limits( rt, 1024 * 1024 * 1024, 1024, 1 );
  add_link_tree[1] = add;
  add_link_tree[2] = Handle<Literal>( 0 );
  add_link_tree[3] = Handle<Literal>( 0 );
  auto handle = rt.create( make_shared<OwnedTree>( std::move( add_link_tree ) ) ).unwrap<ValueTree>();
  client->execute( Handle<Eval>( Handle<Application>( handle::upcast( handle ) ) ) );

  auto invocations = OwnedMutTree::allocate( input_tree->size() );
  for ( size_t i = 0; i < input_tree->size(); i++ ) {
    auto add_tree = OwnedMutTree::allocate( 4 );
    auto v = Handle<Identification>( input_tree->at( i ).unwrap<Expression>().unwrap<Object>().unwrap<Value>() );
    add_tree[0] = make_limits( rt, 1024 * 1024 * 1024, 1024, 1 );
    add_tree[1] = add;
    add_tree[2] = Handle<Strict>( v );
    add_tree[3] = Handle<Strict>( v );
    auto handle = rt.create( make_shared<OwnedTree>( std::move( add_tree ) ) ).unwrap<ValueTree>();
    invocations[i] = Handle<Application>( handle::upcast( handle ) );
  }

  auto tree = rt.create( make_shared<OwnedTree>( std::move( invocations ) ) ).unwrap<ObjectTree>();

  client->send_job( Handle<Eval>( tree ) );

  cerr << "Executing." << endl;
  struct timespec before, after;
  struct timespec before_real, after_real;
  if ( system( "head -n 1 /proc/stat >> ~/stat" ) ) {
    perror( "system" );
    exit( 1 );
  }
  if ( clock_gettime( CLOCK_REALTIME, &before_real ) ) {
    perror( "clock_gettime" );
    exit( 1 );
  }
  if ( clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &before ) ) {
    perror( "clock_gettime" );
    exit( 1 );
  }

  auto res = client->relater_.execute( Handle<Eval>( tree ) );

  if ( clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &after ) ) {
    perror( "clock_gettime" );
    exit( 1 );
  }
  if ( clock_gettime( CLOCK_REALTIME, &after_real ) ) {
    perror( "clock_gettime" );
    exit( 1 );
  }
  if ( system( "head -n 1 /proc/stat >> ~/stat" ) ) {
    perror( "system" );
    exit( 1 );
  }

  double delta = after.tv_sec - before.tv_sec + (double)( after.tv_nsec - before.tv_nsec ) * 1e-9;
  double delta_real
    = after_real.tv_sec - before_real.tv_sec + (double)( after_real.tv_nsec - before_real.tv_nsec ) * 1e-9;

  // print the result
  cerr << "Result: " << res << endl;
  cerr << "Handle: " << res.content << endl;
  if ( argc == 3 ) {
    cerr << "CPU Time: " << delta << " seconds" << endl;
  }
  cerr << "Real Time: " << delta_real << " seconds" << endl;

  return 0;
}
