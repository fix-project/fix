#include <charconv>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

#include "eventloop.hh"
#include "http_server.hh"
#include "socket.hh"
#include "tester-utils.hh"

using namespace std;

static constexpr size_t mebi = 1024 * 1024;

void program_body( span_view<char*> args )
{
  ios::sync_with_stdio( false );

  TCPSocket server_socket {};
  server_socket.set_reuseaddr();
  server_socket.bind( { "0", args.at( 1 ) } );
  server_socket.listen();

  cerr << "Listening on port " << server_socket.local_address().port() << "\n";

  TCPSocket connection = server_socket.accept();
  cerr << "Connection received from " << connection.peer_address().to_string() << "\n";

  EventLoop events;

  RingBuffer client_buffer { mebi }, server_buffer { mebi };

  HTTPServer http_server;
  HTTPRequest request_in_progress;

  events.add_rule(
    "Read bytes from client",
    connection,
    Direction::In,
    [&] { client_buffer.push_from_fd( connection ); },
    [&] { return not client_buffer.writable_region().empty(); } );

  const string response_str = "Hello, user.";

  events.add_rule(
    "Parse bytes from client buffer",
    [&] {
      if ( http_server.read( client_buffer, request_in_progress ) ) {
        cerr << "Got request: " << request_in_progress.request_target << "\n";
        HTTPResponse the_response;
        the_response.http_version = "HTTP/1.1";
        the_response.status_code = "200";
        the_response.reason_phrase = "OK";
        the_response.headers.content_type = "text/html";
        the_response.headers.content_length = response_str.size();
        the_response.body = response_str;
        http_server.push_response( move( the_response ) );
      }
    },
    [&] { return not client_buffer.readable_region().empty(); } );

  events.add_rule(
    "Serialize bytes from server into server buffer",
    [&] { http_server.write( server_buffer ); },
    [&] { return not http_server.responses_empty() and not server_buffer.writable_region().empty(); } );

  events.add_rule(
    "Write bytes to client",
    connection,
    Direction::Out,
    [&] { server_buffer.pop_to_fd( connection ); },
    [&] { return not server_buffer.readable_region().empty(); } );

  while ( events.wait_next_event( -1 ) != EventLoop::Result::Exit ) {}

  cerr << "Exiting...\n";
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " PORT\n";
}

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  try {
    if ( argc != 2 ) {
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
