#include "eventloop.hh"
#include "socket.hh"
#include "spans.hh"

#include <cstdlib>
#include <iostream>
#include <list>

using namespace std;

class Connection
{
  TCPSocket sock_;
  size_t id_;
  Address peer_;
  bool dead_ {};

public:
  void do_read()
  {
    array<char, 64> buf;
    string_span buf_span { buf.data(), buf.size() };
    auto len = sock_.read( buf_span );
    buf_span = buf_span.substr( 0, len );
    cerr << "Got " << len << " bytes from connection " << id_ << " connected to " << peer_.to_string() << "\n";
  }

  Connection( size_t id, TCPSocket&& sock, EventLoop& loop, size_t read_category )
    : sock_( std::move( sock ) )
    , id_( id )
    , peer_( sock_.peer_address() )
  {
    sock_.set_blocking( false );

    loop.add_rule(
      read_category, sock_, Direction::In, [&] { do_read(); }, [] { return true; }, [&] { dead_ = true; } );
  }

  bool dead() const { return dead_; }
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

  const auto socket_read_category = loop.add_category( "socket read" );

  list<Connection> connections;

  size_t connection_id_counter {};

  loop.add_rule( "new TCP connection", server_socket, Direction::In, [&] {
    connections.emplace_back( connection_id_counter++, server_socket.accept(), loop, socket_read_category );
  } );

  while ( loop.wait_next_event( 2000 ) != EventLoop::Result::Exit ) {
    connections.remove_if( []( auto& x ) { return x.dead(); } );

    cerr << "connection_id_counter: " << connection_id_counter << "\n";
    cerr << "active connection count: " << connections.size() << "\n";
    cerr << "\n\n\n###\n\n\n";
  }

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
