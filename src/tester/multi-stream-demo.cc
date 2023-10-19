#include "eventloop.hh"
#include "http_server.hh"
#include "ring_buffer.hh"
#include "socket.hh"
#include "spans.hh"

#include <cstdlib>
#include <iostream>
#include <list>

using namespace std;

struct EventCategories
{
  size_t read, respond, write;

  EventCategories( EventLoop& loop )
    : read( loop.add_category( "socket read" ) )
    , respond( loop.add_category( "HTTP response" ) )
    , write( loop.add_category( "socket write" ) )
  {}
};

class Connection
{
  TCPSocket sock_;
  size_t id_;
  Address peer_;
  bool dead_ {};

  RingBuffer incoming_ { 1024 * 1024 };
  RingBuffer outgoing_ { 1024 * 1024 };

  HTTPServer http_server_ {};
  HTTPRequest req_in_progress_ {};

  vector<EventLoop::RuleHandle> rules_ {};

  void handle_request( const HTTPRequest& request )
  {
    HTTPResponse reply;
    reply.http_version = "HTTP/1.1";
    reply.status_code = "200";
    reply.reason_phrase = "OK";
    reply.headers.content_type = "text/plain";
    reply.body = "Thank you for visiting our website!!! Your connection is ID " + to_string( id_ )
                 + ". Your request had a body size of " + to_string( request.body.size() ) + " bytes.";
    reply.headers.content_length = reply.body.size();
    http_server_.push_response( move( reply ) );
  }

public:
  Connection( size_t id, TCPSocket&& sock, EventLoop& loop, const EventCategories& categories )
    : sock_( std::move( sock ) )
    , id_( id )
    , peer_( sock_.peer_address() )
  {
    sock_.set_blocking( false );

    rules_.push_back( loop.add_rule(
      categories.read,
      sock_,
      Direction::In,
      [&] {
        incoming_.push_from_fd( sock_ );
        if ( http_server_.read( incoming_, req_in_progress_ ) ) {
          handle_request( req_in_progress_ );
        }
      },
      [&] { return incoming_.can_write(); },
      [&] { dead_ = true; } ) );

    rules_.push_back( loop.add_rule(
      categories.respond,
      [&] { http_server_.write( outgoing_ ); },
      [&] { return outgoing_.can_write() and not http_server_.responses_empty(); } ) );

    rules_.push_back( loop.add_rule(
      categories.write,
      sock_,
      Direction::Out,
      [&] { outgoing_.pop_to_fd( sock_ ); },
      [&] { return outgoing_.can_read(); },
      [&] { dead_ = true; } ) );
  }

  bool dead() const { return dead_; }

  ~Connection()
  {
    for ( auto& rule : rules_ ) {
      rule.cancel();
    }
    rules_.clear(); // XXX???XXX
  }
};

void program_body( span_view<char*> args )
{
  ios::sync_with_stdio( false );

  TCPSocket server_socket {};
  server_socket.set_reuseaddr();
  server_socket.bind( { "0", args.at( 1 ) } );
  server_socket.set_blocking( false );
  server_socket.listen();

  cerr << "Listening on port " << server_socket.local_address().port() << "\n";

  EventLoop loop;
  EventCategories event_categories { loop };

  list<Connection> connections;

  size_t connection_id_counter {};

  loop.add_rule( "new TCP connection", server_socket, Direction::In, [&] {
    connections.emplace_back( connection_id_counter++, server_socket.accept(), loop, event_categories );
  } );

  do {
    connections.remove_if( []( auto& x ) { return x.dead(); } );
    cerr << "\033[H\033[2J\033[3J"; // clear screen
    cerr << "connection_id_counter: " << connection_id_counter << "\n";
    cerr << "active connection count: " << connections.size() << "\n";
    loop.summary( cerr );
  } while ( loop.wait_next_event( 500 ) != EventLoop::Result::Exit );

  cerr << "Exiting...\n";
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " service/port\n";
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
