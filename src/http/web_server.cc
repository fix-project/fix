#include "web_server.hh"
#include "exception.hh"

#include <iostream>

using namespace std;

EventCategories::EventCategories( EventLoop& loop )
  : read_from_client( loop.add_category( "Read from client" ) )
  , parse_http( loop.add_category( "Parse HTTP request" ) )
  , serialize_http( loop.add_category( "Serialize HTTP response" ) )
  , write_to_client( loop.add_category( "Send to client" ) )
{}

void WebServer::Client::mark_dead()
{
  good_ = false;
  *cull_needed_ = true;
}

WebServer::Client::Client( TCPSocket& listening_socket,
                           EventLoop& events,
                           const EventCategories& categories,
                           std::shared_ptr<bool> cull_needed,
                           const Handler& handler )
  : socket_( listening_socket.accept() )
  , peer_( socket_.peer_address() )
  , cull_needed_( cull_needed )
  , handler_( handler )
{
  rules_.emplace_back( events.add_rule(
    categories.read_from_client,
    socket_,
    Direction::In,
    [&] { from_client_.push_from_fd( socket_ ); },
    [&] { return good() and from_client_.can_write(); },
    [&] { mark_dead(); } ) );

  rules_.emplace_back( events.add_rule(
    categories.parse_http,
    [&] {
      if ( http_server_.read( from_client_, request_ ) ) {
        handler_( request_, response_ );
        http_server_.push_response( move( response_ ) );
      }
    },
    [&] { return good() and from_client_.can_read(); } ) );

  rules_.emplace_back( events.add_rule(
    categories.serialize_http,
    [&] { http_server_.write( from_server_ ); },
    [&] { return good() and from_server_.can_write() and not http_server_.responses_empty(); } ) );

  rules_.emplace_back( events.add_rule(
    categories.write_to_client,
    socket_,
    Direction::Out,
    [&] { from_server_.pop_to_fd( socket_ ); },
    [&] { return good() and from_server_.can_read(); } ) );

  cerr << "New connection from " << peer_.to_string() << "\n";
}

WebServer::Client::~Client()
{
  cerr << "Connection ended from " << peer_.to_string() << "\n";
  for ( auto& rule : rules_ ) {
    rule.cancel();
  }
}

WebServer::WebServer( EventLoop& events, const uint16_t proposed_port, const Handler& handler )
  : categories_( events )
  , cull_needed_( make_shared<bool>() )
  , handler_( handler )
{
  server_socket_.set_reuseaddr();

  try {
    server_socket_.bind( { "0", proposed_port } );
  } catch ( ... ) {
    server_socket_.bind( { "0", 0 } );
  }

  server_socket_.listen();
  server_socket_.set_blocking( false );

  cerr << "Listening on port " << server_socket_.local_address().port() << "\n";

  events.add_rule( "New connection", server_socket_, Direction::In, [&] {
    clients_.emplace_back( server_socket_, events, categories_, cull_needed_, handler_ );
  } );

  events.add_rule(
    "Cull dead connections",
    [&] {
      clients_.remove_if( []( const auto& client ) { return not client.good(); } );
      *cull_needed_ = false;
    },
    [&] { return *cull_needed_; } );
}
