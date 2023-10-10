#pragma once

#include <concurrentqueue/concurrentqueue.h>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include "base64.hh"
#include "channel.hh"
#include "eventloop.hh"
#include "fixcache.hh"
#include "handle.hh"
#include "interface.hh"
#include "message.hh"
#include "ring_buffer.hh"
#include "socket.hh"
#include "spans.hh"
#include "task.hh"

using ResultChannel = Channel<std::pair<Task, Handle>>;
using WorkChannel = Channel<Task>;
using MessageQueue = moodycamel::ConcurrentQueue<std::pair<uint32_t, Message>>;

struct EventCategories
{
  size_t server_new_socket;
  size_t server_new_connection;
  size_t client_new_connection;
  size_t rx_read_data;
  size_t rx_parse_msg;
  size_t rx_process_msg;
  size_t tx_serialize_msg;
  size_t tx_write_data;
  size_t forward_msg;
};

class Remote : public ITaskRunner
{
  EventLoop& events_;
  EventCategories categories_;
  TCPSocket socket_;

  RingBuffer rx_data_ { 8192 };
  RingBuffer tx_data_ { 8192 };

  MessageParser rx_messages_ {};
  std::queue<Message> tx_messages_ {};

  std::string current_msg_header_ {};
  std::string_view current_msg_unsent_header_ {};
  std::string_view current_msg_unsent_payload_ {};

  std::vector<EventLoop::RuleHandle> installed_rules_ {};

  size_t index_ {};
  std::shared_ptr<MessageQueue> msg_q_ {};

  std::optional<ITaskRunner::Info> info_ {};

  absl::flat_hash_map<Task, size_t, absl::Hash<Task>>& reply_to_;
  std::shared_mutex& mutex_;

  IRuntime& runtime_;

public:
  Remote( EventLoop& events,
          EventCategories categories,
          TCPSocket socket,
          size_t index,
          std::shared_ptr<MessageQueue> msg_q,
          IRuntime& runtime,
          absl::flat_hash_map<Task, size_t, absl::Hash<Task>>& reply_to,
          std::shared_mutex& mutex );

  std::optional<Handle> start( Task&& task ) override;
  std::optional<ITaskRunner::Info> get_info() override { return info_; }

  void push_message( Message&& msg );

  bool is_connected();
  Address local_address() { return socket_.local_address(); }
  Address peer_address() { return socket_.peer_address(); }

private:
  void load_tx_message();
  void write_to_rb();
  void read_from_rb();
  void install_rule( EventLoop::RuleHandle rule ) { installed_rules_.push_back( rule ); }
  void process_incoming_message( Message&& msg );
};

class NetworkWorker : public IResultCache
{
private:
  EventCategories categories_ {};

  EventLoop events_ {};
  std::thread network_thread_;
  std::atomic<bool> should_exit_ = false;

  Channel<TCPSocket> listening_sockets_ {};
  Channel<TCPSocket> connecting_sockets_ {};

  std::vector<Remote> connections_ {};
  std::vector<TCPSocket> server_sockets_ {};

  std::shared_ptr<MessageQueue> msg_q_ { std::make_shared<MessageQueue>() };
  IRuntime& runtime_;

  absl::flat_hash_map<Task, size_t, absl::Hash<Task>> reply_to_ {};
  std::shared_mutex mutex_ {};

  void run_loop();

public:
  NetworkWorker( IRuntime& runtime )
    : network_thread_( std::bind( &NetworkWorker::run_loop, this ) )
    , runtime_( runtime ) {};

  ~NetworkWorker()
  {
    should_exit_ = true;
    network_thread_.join();
  }

  Address start_server( const Address& address )
  {
    std::cout << "Start server." << std::endl;
    TCPSocket socket;
    socket.set_reuseaddr();
    socket.bind( address );
    socket.listen();
    socket.set_blocking( false );
    Address listen_address = socket.local_address();
    listening_sockets_ << std::move( socket );
    return listen_address;
  }

  void connect( const Address& address )
  {
    std::cout << "Connect." << std::endl;
    TCPSocket socket;
    socket.connect( address );
    connecting_sockets_ << std::move( socket );
  }

  void finish( Task&& task, Handle result ) override;
};
