#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unistd.h>

#include "handle.hh"
#include "handle_post.hh"
#include "object.hh"
#include "runtimes.hh"
#include "scheduler.hh"
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
  /* if ( argc != 4 ) { */
  /*   cerr << "Usage: fixpoint-client-wikipedia needle-string haystack-label +[address]:[port]" << endl; */
  /*   exit( 1 ); */
  /* } */
  if ( argc != 3 ) {
    cerr << "Usage: fixpoint-client-wikipedia needle-string haystack-label" << endl;
    exit( 1 );
  }

  auto needle_string = std::string( argv[1] );
  auto haystack_label = std::string( argv[2] );

  /* std::shared_ptr<Client> client; */
  /* if ( argv[3][0] == '+' ) { */
  /*   string addr( &argv[3][1] ); */
  /*   if ( addr.find( ':' ) == string::npos ) { */
  /*     throw runtime_error( "invalid argument " + addr ); */
  /*   } */
  /*   Address address( addr.substr( 0, addr.find( ':' ) ), stoi( addr.substr( addr.find( ':' ) + 1 ) ) ); */
  /*   client = Client::init( address ); */
  /* } else { */
  /*   exit( 1 ); */
  /* } */

  shared_ptr<Scheduler> scheduler = make_shared<HintScheduler>();
  auto client = std::make_shared<Relater>( std::thread::hardware_concurrency(), std::nullopt, scheduler );
  IRuntime& rt = *client;

  auto mapreduce = client->execute(
    Handle<Eval>( compile( rt, "build/applications-prefix/src/applications-build/mapreduce/mapreduce.wasm" ) ) );
  auto mapper = client->execute( Handle<Eval>(
    compile( rt, "build/applications-prefix/src/applications-build/count-words/count_words.wasm" ) ) );
  auto reducer = client->execute( Handle<Eval>(
    compile( rt, "build/applications-prefix/src/applications-build/count-words/merge_counts.wasm" ) ) );

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

  const size_t TOTAL_SIZE = handle::size( haystack );
  const size_t LEAF_SIZE = 1024 * 1024;
  const size_t LEAVES = std::ceil( TOTAL_SIZE / (double)LEAF_SIZE );
  cout << "running as " << LEAVES << " parallel counts" << endl;
  auto args = OwnedMutTree::allocate( LEAVES );
  for ( size_t i = 0; i < LEAVES; i++ ) {
    auto selection = OwnedMutTree::allocate( 3 );
    selection[0] = haystack;
    selection[1] = Handle<Literal>( LEAF_SIZE * i );
    selection[2] = Handle<Literal>( std::min( LEAF_SIZE * ( i + 1 ), TOTAL_SIZE ) );
    auto handle = rt.create( make_shared<OwnedTree>( std::move( selection ) ) ).unwrap<ValueTree>();
    auto select = Handle<Selection>( Handle<ObjectTree>( handle ) );

    auto arg_tree = OwnedMutTree::allocate( 2 );
    arg_tree[0] = needle;
    arg_tree[1] = Handle<Shallow>( select );
    args[i] = rt.create( make_shared<OwnedTree>( std::move( arg_tree ) ) ).unwrap<ExpressionTree>();
  }

  auto application = OwnedMutTree::allocate( 7 );
  application[0] = make_limits( rt, 1024 * 1024 * 1024, 1024 * 1024, 1, false );
  application[1] = mapreduce;
  application[2] = mapper;
  application[3] = reducer;
  application[4] = rt.create( make_shared<OwnedTree>( std::move( args ) ) ).unwrap<ExpressionTree>();
  application[5] = make_limits( rt, 1024 * 1024 * 1024, 256 * 8, 1, false );
  application[6] = make_limits( rt, 1024 * 1024 * 1024, 256 * 8, 1, true );

  auto handle = rt.create( make_shared<OwnedTree>( std::move( application ) ) ).unwrap<ExpressionTree>();
  auto res = client->execute( Handle<Eval>( Handle<Object>( Handle<Application>( handle::upcast( handle ) ) ) ) );

  // print the result
  cerr << "Result:\n" << res << endl;
  cerr << "Result:\n" << res.content << endl;

  return 0;
}
