#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "base64.hh"
#include "tester-utils.hh"

#define TRUSTED_COMPILE_ENCODE Handle( base64::decode( COMPILE_ENCODE ) )

using namespace std;
using path = filesystem::path;

class RemoteServer
{
  string server_;
  string socket_;
  thread thread_;
  bool destructing_ = false;

  void spawn_ssh_socket()
  {
    cerr << "Connecting to " << server_ << " over SSH.  You may need to authenticate.\n";
    stringstream ss;
    ss << "ssh -N -M -S '" << socket_ << "' '" << server_ << "'";
    int code = system( ss.str().c_str() );
    (void)code;
  }

public:
  RemoteServer( string server )
    : server_( server )
    , socket_( filesystem::temp_directory_path()
               / filesystem::path( "fix-ssh-" + to_string( getpid() ) + "." + to_string( rand() ) ) )
    , thread_( &RemoteServer::spawn_ssh_socket, this )
  {}

  int run( const string& command )
  {
    return system( ( "ssh -S '" + socket_ + "' '" + server_ + "' '" + command + "'" ).c_str() );
  }

  string run_and_capture_stdout( const string& command )
  {
    FILE* p = popen( ( "ssh -S '" + socket_ + "' '" + server_ + "' '" + command + "'" ).c_str(), "r" );
    stringstream ss;

    while ( !feof( p ) ) {
      array<char, 1024> buf;
      size_t bytes = fread( buf.data(), 1, buf.size(), p );
      ss << string_view( buf.begin(), bytes );
    }
    int status = WEXITSTATUS( pclose( p ) );
    if ( status != 0 ) {
      throw runtime_error( "command exited with non-zero status " + to_string( status ) + ": " + command );
    }
    return ss.str();
  }

  void transmit_directory( filesystem::path local, filesystem::path remote )
  {
    int result = system( ( "rsync -e \"ssh -S '" + socket_ + "'\" -r '" + local.string() + "/' '" + server_ + ":"
                           + remote.string() + "'" )
                           .c_str() );
    if ( result ) {
      throw runtime_error( "failed to transmit directory" );
    }
  }

  void receive_directory( filesystem::path local, filesystem::path remote )
  {
    int result = system( ( "rsync -e \"ssh -S '" + socket_ + "'\" -r '" + server_ + ":" + remote.string() + "/'  '"
                           + local.string() + "'" )
                           .c_str() );
    if ( result ) {
      throw runtime_error( "failed to receive directory" );
    }
  }

  void disconnect()
  {
    if ( filesystem::exists( socket_ ) ) {
      int result = system( ( "ssh -S '" + socket_ + "' -O exit '" + server_ + "'" ).c_str() );
      if ( !destructing_ and result ) {
        throw runtime_error( "failed to receive directory" );
      }
      thread_.join();
    }
  }

  ~RemoteServer()
  {
    destructing_ = true;
    disconnect();
  }
};

void program_body( span_view<char*> args )
{
  auto& runtime = RuntimeStorage::get_instance();
  runtime.deserialize();
  ios::sync_with_stdio( false );

  args.remove_prefix( 1 );

  vector<tuple<string, int, int>> nodes;

  // format: username@server:cores:bps
  // e.g. username@stagecast.org/fix:256:100000
  nodes.emplace_back( "localhost", std::thread::hardware_concurrency(), 0 );

  while ( args.size() > 0 and args.at( 0 )[0] == '+' ) {
    string arg = args[0];
    args.remove_prefix( 1 );
    string server = arg.substr( 1, arg.find( ':' ) - 1 );
    arg = arg.substr( arg.find( ':' ) + 1 );
    string cores = arg.substr( 0, arg.find( ':' ) );
    string bps = arg.substr( arg.find( ':' ) + 1 );
    nodes.emplace_back( server, stoi( cores ), stoi( bps ) );
  }

  ReadOnlyFile scheduler_file( args[0] );

  Handle scheduler_blob = runtime.add_blob( string_view( scheduler_file ) );

  Tree scheduler_compile { 3 };
  scheduler_compile.at( 0 ) = Handle( "unused" );
  scheduler_compile.at( 1 ) = TRUSTED_COMPILE_ENCODE;
  scheduler_compile.at( 2 ) = scheduler_blob;
  Handle scheduler_compile_thunk = runtime.add_thunk( Thunk( runtime.add_tree( std::move( scheduler_compile ) ) ) );

  args.remove_prefix( 1 );

  vector<ReadOnlyFile> open_files;

  Handle program_encode = parse_args( args, open_files );

  Tree scheduler_data { (uint32_t)nodes.size() };
  for ( size_t i = 0; i < nodes.size(); i++ ) {
    Tree node_data { 2 };
    node_data.at( 0 ) = Handle( std::get<1>( nodes[i] ) );
    node_data.at( 1 ) = Handle( std::get<2>( nodes[i] ) );
    scheduler_data.at( i ) = runtime.add_tree( std::move( node_data ) );
  }

  Tree scheduler_run { 4 };
  scheduler_run.at( 0 ) = Handle( "unused" );
  scheduler_run.at( 1 ) = scheduler_compile_thunk;
  scheduler_run.at( 2 ) = runtime.add_tree( std::move( scheduler_data ) );
  scheduler_run.at( 3 ) = program_encode;
  Handle scheduler_run_thunk = runtime.add_thunk( Thunk( runtime.add_tree( std::move( scheduler_run ) ) ) );

  Handle scheduler_result = runtime.eval_thunk( scheduler_run_thunk );

  if ( !scheduler_result.is_literal_blob() || scheduler_result.get_length() != sizeof( uint32_t ) ) {
    throw runtime_error( "scheduler did not return an i32" );
  }
  uint32_t chosen_index;
  memcpy( &chosen_index, scheduler_result.literal_blob().data(), 4 );

  if ( chosen_index > nodes.size() ) {
    throw runtime_error( "scheduler chose " + to_string( chosen_index ) + ", which is not a valid node" );
  }

  filesystem::path temp_repo
    = filesystem::temp_directory_path()
      / filesystem::path( "fix-repo-" + to_string( getpid() ) + "." + to_string( rand() ) );
  filesystem::create_directories( temp_repo );
  string name = runtime.serialize_to_dir( program_encode, temp_repo );

  cerr << "Connecting...\n";
  RemoteServer server( get<0>( nodes[chosen_index] ) );
  string dirname = server.run_and_capture_stdout( "mktemp -d" );
  dirname = dirname.substr( 0, dirname.size() - 1 );
  filesystem::path remote_dir( dirname );
  filesystem::path remote_repo = remote_dir / ".fix";
  cerr << "Uploading...\n";
  server.transmit_directory( filesystem::path( temp_repo ), remote_repo );
  filesystem::remove_all( temp_repo );
  cerr << "Evaluating...\n";
  filesystem::path remote_output( server.run_and_capture_stdout( "~/fix/build/src/tester/distributed-worker '"
                                                                 + remote_repo.string() + "' '" + name + "'" ) );
  string filename = remote_output.filename();
  filesystem::path remote_output_repo = remote_output.parent_path();
  server.run( "rm -r '" + remote_dir.string() + "'" );
  filesystem::path output_repo = temp_repo.concat( "-output" );
  cerr << "Downloading...\n";
  server.receive_directory( output_repo, remote_output_repo );
  server.run( "rm -r '" + remote_output_repo.string() + "'" );

  runtime.deserialize_from_dir( output_repo );

  cout << pretty_print( Handle( base64::decode( filename ) ) ) << "\n";

  filesystem::remove_all( output_repo );

  server.disconnect();
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " +user@server:cores:bps... scheduler entry...\n";
  cerr << "   scheduler: a filename of a wasm blob corresponding to a valid scheduler program\n";
  cerr << "   entry :=   file:<filename>\n";
  cerr << "            | string:<string>\n";
  cerr << "            | name:<base64-encoded name>\n";
  cerr << "            | uint<n>:<integer> (with <n> = 8 | 16 | 32 | 64)\n";
  cerr << "            | tree:<n> (followed by <n> entries)\n";
  cerr << "            | thunk: (followed by tree:<n>)\n";
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
