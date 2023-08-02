#pragma once

#include "address.hh"
#include "eventloop.hh"
#include "file_descriptor.hh"
#include "http_server.hh"
#include "ring_buffer.hh"
#include "socket.hh"

#include <list>
#include <memory>
#include <vector>

struct EventCategories
{
  size_t read_from_client, parse_http, serialize_http, write_to_client;

  EventCategories( EventLoop& loop );
};

class WebServer
{
public:
  using Handler = std::function<void( const HTTPRequest& request, HTTPResponse& response )>;

private:
  class Client
  {
    static constexpr size_t mebi = 1024 * 1024;

    TCPSocket socket_;
    Address peer_;
    RingBuffer from_client_ { mebi }, from_server_ { mebi };
    std::shared_ptr<bool> cull_needed_;
    bool good_ { true };
    std::vector<EventLoop::RuleHandle> rules_ {};
    Handler handler_;

    HTTPServer http_server_ {};
    HTTPRequest request_ {};
    HTTPResponse response_ {};

    void mark_dead();

  public:
    Client( TCPSocket& listening_socket,
            EventLoop& events,
            const EventCategories& categories,
            std::shared_ptr<bool> cull_needed,
            const Handler& handler );

    ~Client();

    bool good() const { return good_; }

    Client( Client&& other ) = default;
  };

  TCPSocket server_socket_ {};
  EventCategories categories_;
  std::list<Client> clients_ {};
  std::shared_ptr<bool> cull_needed_;
  Handler handler_;

public:
  WebServer( EventLoop& events, const uint16_t proposed_port, const Handler& handler );
};
