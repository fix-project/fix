#include <memory>
#include <stdio.h>

#include "relater.hh"
#include "test.hh"

#include "option-parser.hh"
#include "relater.hh"
#include "scheduler.hh"

#include <filesystem>
#include <glog/logging.h>
#include <memory>

using namespace std;
namespace fs = std::filesystem;

void run( std::shared_ptr<Relater>, string python_wasm_path, string workingdir, string command_line );

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
    size_t count = 0;
    for ( auto& _ : std::filesystem::directory_iterator( path ) ) {
      count += 1;
    }
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
  size_t count = 0;
  for ( auto& _ : std::filesystem::directory_iterator( python_src ) ) {
    count += 1;
  }
  OwnedMutTree tree = OwnedMutTree::allocate( count );
  size_t i = 0;
  for ( auto& subpath : fs::directory_iterator( python_src ) ) {
    tree[i] = create_from_path( subpath );
    i++;
  }
  return dirent( ".",
                 d,
                 tester::Tree( dirent(
                   "workingdir",
                   d,
                   handle::upcast( tester::rt->create( std::make_shared<OwnedTree>( std::move( tree ) ) ) ) ) ) );
}

Handle<Fix> cl( string_view command_line )
{
  vector<string_view> args;

  unsigned int field_start = 0;
  for ( unsigned int i = 0; i < command_line.size(); i++ ) {
    if ( command_line[i] == ' ' ) {
      args.push_back( command_line.substr( field_start, i - field_start ) );
      field_start = i + 1;
    }
  }
  args.push_back( command_line.substr( field_start ) );

  auto tmp = OwnedMutTree::allocate( args.size() );
  unsigned int i = 0;
  for ( auto& arg : args ) {
    tmp[i] = tester::Blob( arg );
    i++;
  }
  return handle::upcast( tester::rt->create( std::make_shared<OwnedTree>( std::move( tmp ) ) ) );
}

void run( std::shared_ptr<Relater> rt, string python_wasm_path, string workingdir, string command_line )
{
  tester::rt = rt;

  auto python_exec = tester::Compile( tester::File( python_wasm_path ) );
  auto dir = filesys( workingdir );
  auto command = cl( command_line );
  auto input = flatware_input( *tester::rt, tester::Limits(), python_exec, dir, command );
  auto result_handle = tester::rt->execute( input );
  auto result_tree = tester::rt->get( result_handle.try_into<ValueTree>().value() ).value();

  uint32_t x = -1;
  memcpy( &x, handle::extract<Literal>( result_tree->at( 0 ) ).value().data(), sizeof( uint32_t ) );
  if ( x != 0 ) {
    fprintf( stderr, "Return failed, returned %d != 0\n", x );
  }

  auto fixstdout = handle::extract<Blob>( result_tree->at( 2 ) ).value();
  auto fixstderr = handle::extract<Blob>( result_tree->at( 3 ) ).value();

  cerr << endl;
  cerr << "exit code: " << x << endl;
  fixstdout.visit<void>(
    overload { [&]( Handle<Named> n ) { cerr << "stdout: " << tester::rt->get( n ).value()->data() << endl; },
               [&]( Handle<Literal> l ) { cerr << "stdout: " << l.data() << endl; } }

  );

  fixstderr.visit<void>(
    overload { [&]( Handle<Named> n ) { cerr << "stderr: " << tester::rt->get( n ).value()->data() << endl; },
               [&]( Handle<Literal> l ) { cerr << "stderr: " << l.data() << endl; } }

  );

  exit( x );
}

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  OptionParser parser( "fixpoint-test", "Run a fixpoint test" );
  optional<string> sche_opt;
  string python_wasm_path;
  string workingdir;
  string command_line;

  parser.AddArgument( "path-to-python.wasm", OptionParser::ArgumentCount::One, [&]( const char* argument ) {
    python_wasm_path = string( argument );
  } );
  parser.AddArgument( "working-directory", OptionParser::ArgumentCount::One, [&]( const char* argument ) {
    workingdir = string( argument );
  } );
  parser.AddArgument( "command-line", OptionParser::ArgumentCount::One, [&]( const char* argument ) {
    command_line = string( argument );
  } );
  parser.AddOption( 's', "scheduler", "scheduler", "Scheduler to use [local, hint]", [&]( const char* argument ) {
    sche_opt = argument;
    if ( not( *sche_opt == "hint" or *sche_opt == "local" ) ) {
      throw runtime_error( "Invalid scheduler: " + sche_opt.value() );
    }
  } );

  google::InitGoogleLogging( argv[0] );
  google::SetStderrLogging( google::INFO );
  google::InstallFailureSignalHandler();

  parser.Parse( argc, argv );

  shared_ptr<Scheduler> scheduler = make_shared<HintScheduler>();
  if ( sche_opt.has_value() ) {
    if ( *sche_opt == "local" ) {
      scheduler = make_shared<LocalScheduler>();
    }
  }

  auto rt = std::make_shared<Relater>( std::thread::hardware_concurrency(), std::nullopt, scheduler );

  run( rt, python_wasm_path, workingdir, command_line );
}
