#include "handle.hh"
#include "interface.hh"
#include "network.hh"
#include "object.hh"
#include "runtimestorage.hh"
#include <memory>
#include <sys/wait.h>
#include <vector>

using namespace std;

Address address( "127.0.0.1", 12345 );

const std::string path = "testing/wasm-examples/addblob.wasm";
const std::string path2 = "testing/wasm-examples/fib.wasm";

class FakeRuntime : public MultiWorkerRuntime
{
  RuntimeStorage storage_ {};

public:
  optional<BlobData> get( Handle<Named> name ) override { return storage_.get( name ); };
  optional<TreeData> get( Handle<AnyTree> name ) override { return storage_.get( name ); };
  optional<Handle<Object>> get( Handle<Relation> name ) override { return storage_.get( name ); };
  optional<Handle<AnyTree>> get_handle( Handle<AnyTree> name ) override { return storage_.get_handle( name ); };

  void put( Handle<Named> name, BlobData data ) override { storage_.create( data, name ); }
  void put( Handle<AnyTree> name, TreeData data ) override { storage_.create( data, name ); }
  void put( Handle<Relation> name, Handle<Object> data ) override { storage_.create( data, name ); }

  bool contains( Handle<Named> handle ) override { return storage_.contains( handle ); }
  bool contains( Handle<AnyTree> handle ) override { return storage_.contains( handle ); }
  bool contains( Handle<Relation> handle ) override { return storage_.contains( handle ); }

  virtual void add_worker( std::shared_ptr<IRuntime> ) override {}
};

template<FixHandle... Args>
Handle<AnyTree> create_tree( FakeRuntime& rt, Args... args )
{
  OwnedMutTree tree = OwnedMutTree::allocate( sizeof...( args ) );
  size_t i = 0;
  (
    [&] {
      tree[i] = args;
      i++;
    }(),
    ... );
  return rt.create( std::make_shared<OwnedTree>( std::move( tree ) ) );
}

Handle<Relation> create_eval( FakeRuntime& rt, Handle<Object> in, Handle<Object> out )
{
  rt.put( Handle<Eval>( in ), out );
  return Handle<Eval>( in );
}

using test_type = variant<Handle<Named>, Handle<AnyTree>, Handle<Relation>>;

vector<test_type> init_rt_set1( FakeRuntime& rt )
{
  vector<test_type> res {};

  BlobData test_blob = make_shared<OwnedBlob>( path );
  res.push_back( rt.create( test_blob ).try_into<Named>().value() );
  res.push_back( create_tree( rt, 1_literal64, 10_literal64 ) );
  res.push_back( create_eval( rt, 7_literal64, 12_literal64 ) );

  return res;
}

vector<test_type> init_rt_set2( FakeRuntime& rt )
{
  vector<test_type> res {};

  BlobData test_blob = make_shared<OwnedBlob>( path2 );
  res.push_back( rt.create( test_blob ).try_into<Named>().value() );
  res.push_back( create_tree( rt, 42_literal64, 70_literal64, 18_literal64 ) );
  res.push_back( create_eval( rt, 17_literal64, 23_literal64 ) );

  return res;
}

void client()
{
  FakeRuntime rt {};
  FakeRuntime rt_check {};

  NetworkWorker nw( rt );
  nw.start();
  nw.connect( address );

  auto tests = init_rt_set1( rt );
  auto checks = init_rt_set2( rt_check );
  auto remote = nw.get_remote( address );

  // Send puts
  for ( const auto& test : tests ) {
    std::visit( [&]( auto h ) { remote->put( h, rt.get( h ).value() ); }, test );
  }

  // Send gets
  for ( const auto& check : checks ) {
    std::visit( [&]( auto h ) { remote->get( h ); }, check );
  }

  sleep( 1 );

  // Test gets
  for ( const auto& check : checks ) {
    auto error = std::visit( [&]( auto h ) { return !rt.contains( h ); }, check );
    if ( error )
      exit( 1 );
  }

  nw.stop();
}

void server( int client_pid )
{
  (void)client_pid;

  FakeRuntime rt {};
  FakeRuntime rt_check {};

  NetworkWorker nw( rt );
  nw.start();
  nw.start_server( address );

  auto tests = init_rt_set2( rt );
  auto checks = init_rt_set1( rt_check );

  while ( kill( client_pid, 0 ) == 0 ) {
    sleep( 1 );
  }

  // Test puts
  for ( const auto& check : checks ) {
    auto error = std::visit( [&]( auto h ) { return !rt.contains( h ); }, check );
    if ( error )
      exit( 1 );
  }

  nw.stop();
}

void test( void )
{
  auto client_pid = fork();
  if ( client_pid ) {
    auto server_pid = fork();
    if ( server_pid ) {
      waitpid( client_pid, NULL, 0 );
      waitpid( server_pid, NULL, 0 );
    } else {
      server( client_pid );
      exit( 0 );
    }
  } else {
    sleep( 1 );
    client();
    exit( 0 );
  }
}
