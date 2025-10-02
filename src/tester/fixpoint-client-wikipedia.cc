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

void progress( double x )
{
  x = std::clamp( x, 0.0, 1.0 );
  struct winsize w;
  ioctl( STDOUT_FILENO, TIOCGWINSZ, &w );
  cerr << "\r";
  cerr << "[";
  for ( int i = 0; i < w.ws_col - 2; i++ ) {
    if ( i / (double)( w.ws_col - 2 ) < x ) {
      cerr << "#";
    } else {
      cerr << " ";
    }
  }
  cerr << "]";
}

int main( int argc, char* argv[] )
{
  if ( argc != 3 && argc != 4 ) {
    cerr << "Usage: fixpoint-client-wikipedia needle-string haystack-label [address:port]" << endl;
    cerr << "\tset the environment variable SMALL_SLICES to use 10 MiB slices (default 1 GiB)" << endl;
    cerr << "\tset the environment variable MEDIUM_SLICES to use 100 MiB slices (default 1 GiB)" << endl;
    exit( 1 );
  }

  auto needle_string = std::string( argv[1] );
  auto haystack_label = std::string( argv[2] );

  /* auto ro_rt = [&] { */
  /*   auto client = ReadOnlyRT::init(); */
  /*   std::shared_ptr<FrontendRT> frontend = dynamic_pointer_cast<FrontendRT>( client ); */
  /*   return make_pair( frontend, std::reference_wrapper( client->get_rt() ) ); */
  /* }; */
  auto rw_rt = [&] {
    auto client = ReadWriteRT::init();
    std::shared_ptr<FrontendRT> frontend = dynamic_pointer_cast<FrontendRT>( client );
    return make_pair( frontend, std::reference_wrapper( client->get_rt() ) );
  };
  auto client_rt = [&] {
    std::shared_ptr<Client> client;
    string addr( argv[3] );
    if ( addr.find( ':' ) == string::npos ) {
      throw runtime_error( "invalid argument " + addr );
    }
    Address address( addr.substr( 0, addr.find( ':' ) ), stoi( addr.substr( addr.find( ':' ) + 1 ) ) );
    client = Client::init( address );
    std::shared_ptr<FrontendRT> frontend = dynamic_pointer_cast<FrontendRT>( client );
    return make_pair( frontend, std::reference_wrapper( client->get_rt() ) );
  };
  auto [client, rt] = argc == 3 ? rw_rt() : client_rt();

  auto mapreduce = rt.labeled( "mapreduce" ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();
  auto mapper = rt.labeled( "count-words" ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();
  auto reducer = rt.labeled( "merge-counts" ).unwrap<Expression>().unwrap<Object>().unwrap<Value>();

  auto needle_blob = OwnedMutBlob::allocate( needle_string.size() );
  memcpy( needle_blob.data(), needle_string.data(), needle_string.size() );
  auto needle = rt.create( std::make_shared<OwnedBlob>( std::move( needle_blob ) ) );

  auto haystack = handle::data( rt.labeled( haystack_label ) )
                    .value()
                    .visit<Handle<BlobRef>>( overload {
                      [&]( Handle<Named> blob ) { return Handle<BlobRef>( blob ); },
                      [&]( Handle<Literal> blob ) { return Handle<BlobRef>( blob ); },
                      [&]( Handle<BlobRef> ref ) { return ref; },
                      [&]( auto ) -> Handle<BlobRef> { throw runtime_error( "haystack label was not a blob" ); },
                    } );

#define KiB 1024
#define MiB ( 1024 * KiB )
#define GiB ( 1024 * MiB )

  const size_t TOTAL_SIZE = handle::size( haystack );
  const size_t LEAF_SIZE
    = getenv( "SMALL_SLICES" ) ? 10 * MiB : ( getenv( "MEDIUM_SLICES" ) ? 100 * MiB : 1 * GiB );
  const size_t LEAVES = std::ceil( TOTAL_SIZE / (double)LEAF_SIZE );
  cerr << "Slice size: " << LEAF_SIZE << " bytes" << endl;
  cerr << "Running " << LEAVES << " parallel maps." << endl;
  bool precompute_only = needle_string.empty();
  if ( precompute_only ) {
    cerr << "Precomputing selections." << endl;
    progress( 0 );
  }
  auto args = OwnedMutTree::allocate( LEAVES );
  for ( size_t i = 0; i < LEAVES; i++ ) {
    if ( precompute_only )
      progress( i / (double)LEAVES );
    auto selection = OwnedMutTree::allocate( 3 );
    selection[0] = haystack;
    selection[1] = Handle<Literal>( LEAF_SIZE * i );
    selection[2] = Handle<Literal>( std::min( LEAF_SIZE * ( i + 1 ), TOTAL_SIZE ) );
    auto handle = rt.create( make_shared<OwnedTree>( std::move( selection ) ) ).unwrap<ValueTree>();
    auto select = Handle<Selection>( Handle<ObjectTree>( handle ) );
    if ( precompute_only ) {
      // force this to execute and be cached
      client->execute( Handle<Think>( select ) );
    }

    auto arg_tree = OwnedMutTree::allocate( 2 );
    arg_tree[0] = needle;
    arg_tree[1] = select;
    args[i] = rt.create( make_shared<OwnedTree>( std::move( arg_tree ) ) ).unwrap<ObjectTree>();
  }

  if ( precompute_only ) {
    cerr << "No search query provided." << endl;
    exit( EXIT_SUCCESS );
  }

  auto tree = rt.create( make_shared<OwnedTree>( std::move( args ) ) ).unwrap<ObjectTree>();

  auto toplevel = OwnedMutTree::allocate( 7 );
  toplevel[0] = make_limits( rt, 1024 * 1024 * 1024, 1024 * 1024, 1, false );
  toplevel[1] = mapreduce;
  toplevel[2] = mapper;
  toplevel[3] = reducer;
  toplevel[4] = Handle<ObjectTreeRef>( tree, LEAVES );
  toplevel[5] = make_limits( rt, 1024 * 1024 * 1024, 256 * 8, 1, false );
  toplevel[6] = make_limits( rt, 1024 * 1024 * 1024, 256 * 8, 1, true );

  auto handle = rt.create( make_shared<OwnedTree>( std::move( toplevel ) ) ).unwrap<ObjectTree>();
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
  auto res = client->execute( Handle<Eval>( Handle<Object>( Handle<Application>( handle::upcast( handle ) ) ) ) );
  if ( clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &after ) ) {
    perror( "clock_gettime" );
    exit( 1 );
  }
  if ( clock_gettime( CLOCK_REALTIME, &after_real ) ) {
    perror( "clock_gettime" );
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
  uint64_t count = uint64_t( handle::extract<Literal>( res ).value() );
  cerr << "Count: " << count << endl;

  return 0;
}
