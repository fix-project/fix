#include <memory>
#include <stdio.h>

#include "relater.hh"
#include "test.hh"

using namespace std;
namespace fs = std::filesystem;

namespace tester {
shared_ptr<Relater> rt;
auto Limits = []() { return limits( *rt, 1024 * 1024 * 1024, 1024 * 1024, 1024 ); };
auto Compile = []( Handle<Fix> wasm ) { return compile( *rt, wasm ); };
auto File = []( std::filesystem::path path ) { return file( *rt, path ); };
auto Tree = []( auto... args ) { return handle::upcast( tree( *rt, args... ) ); };
auto Blob = []( std::string_view contents ) { return blob( *rt, contents ); };
}

Handle<Fix> dirent( string_view name, string_view permissions, Handle<Fix> content )
{
  return tester::Tree( tester::Blob( name ), tester::Blob( permissions ), content );
}

static string d = "040000";
static string f = "100644";

Handle<Fix> create_from_path( fs::path path )
{
  if ( fs::is_regular_file( path ) ) {
    return dirent( path.filename().string(), f, tester::File( path ) );
  } else if ( fs::is_directory( path ) ) {
    size_t count = distance( fs::directory_iterator( path ), {} );
    OwnedMutTree tree = OwnedMutTree::allocate( count );

    size_t i = 0;
    for ( auto& subpath : fs::directory_iterator( path ) ) {
      tree[i] = create_from_path( subpath );
      i++;
    }

    return dirent( path.filename().string(),
                   d,
                   handle::upcast( tester::rt->create( std::make_shared<OwnedTree>( std::move( tree ) ) ) ) );
  } else {
    throw std::runtime_error( "Invalid file type" );
  }
}

Handle<Fix> filesys( string python_src_path )
{
  fs::path python_src { python_src_path };
  size_t count = distance( fs::directory_iterator( python_src ), {} );
  OwnedMutTree tree = OwnedMutTree::allocate( count + 1 );
  size_t i = 0;
  for ( auto& subpath : fs::directory_iterator( python_src ) ) {
    tree[i] = create_from_path( subpath );
    i++;
  }
  tree[count] = dirent( "main.py", f, tester::Blob( "print(2 + 2)" ) );
  return dirent( ".", d, handle::upcast( tester::rt->create( std::make_shared<OwnedTree>( std::move( tree ) ) ) ) );
}

void test( shared_ptr<Relater> rt )
{
  tester::rt = rt;

  auto python_exec = tester::Compile(
    tester::File( "applications-prefix/src/applications-build/flatware/examples/python/python-fixpoint.wasm" ) );
  auto dir = filesys( "applications-prefix/src/applications-build/flatware/examples/python/python-workingdir" );
  auto input = flatware_input( *tester::rt,
                               tester::Limits(),
                               python_exec,
                               dir,
                               tester::Tree( tester::Blob( "python" ), tester::Blob( "main.py" ) ) );
  auto result_handle = tester::rt->execute( input );
  auto result_tree = tester::rt->get( result_handle.try_into<ValueTree>().value() ).value();

  auto fixstdout = handle::extract<Blob>( result_tree->at( 2 ) ).value();
  auto fixstderr = handle::extract<Blob>( result_tree->at( 3 ) ).value();

  fixstdout.visit<void>(
    overload { [&]( Handle<Named> n ) { cerr << "stdout: " << tester::rt->get( n ).value()->data() << endl; },
               [&]( Handle<Literal> l ) { cerr << "stdout: " << l.data() << endl; } }

  );

  fixstderr.visit<void>(
    overload { [&]( Handle<Named> n ) { cerr << "stderr: " << tester::rt->get( n ).value()->data() << endl; },
               [&]( Handle<Literal> l ) { cerr << "stderr: " << l.data() << endl; } }

  );

  uint32_t x = -1;
  memcpy( &x, handle::extract<Literal>( result_tree->at( 0 ) ).value().data(), sizeof( uint32_t ) );
  if ( x != 0 ) {
    fprintf( stderr, "Return failed, returned %d != 0\n", x );
    exit( 1 );
  }

  auto out = handle::extract<Literal>( result_tree->at( 2 ) );
  if ( not out.has_value() ) {
    fprintf( stderr, "Output failed\n" );
    exit( 1 );
  }
  auto out_string = std::string_view( out.value().data(), out.value().size() );
  if ( out_string != "4\n" ) {
    fprintf( stderr, "Output failed, returned %s != 4\n", out_string.data() );
    exit( 1 );
  }
}
