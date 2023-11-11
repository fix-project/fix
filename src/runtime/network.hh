#pragma once

#include <concurrentqueue/concurrentqueue.h>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

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
using MessageQueue = moodycamel::ConcurrentQueue<std::pair<uint32_t, MessagePayload>>;

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

class Runtime;

class Remote : public ITaskRunner
{
  EventLoop& events_;
  TCPSocket socket_;

  MessageQueue& msg_q_;
  Runtime& runtime_;

  RingBuffer rx_data_ { 8192 };
  RingBuffer tx_data_ { 8192 };

  MessageParser rx_messages_ {};
  std::queue<OutgoingMessage> tx_messages_ {};

  std::string current_msg_header_ {};
  std::string_view current_msg_unsent_header_ {};
  std::string_view current_msg_unsent_payload_ {};

  std::vector<EventLoop::RuleHandle> installed_rules_ {};

  size_t index_ {};

  std::optional<ITaskRunner::Info> info_ {};

  absl::flat_hash_map<Task, size_t, absl::Hash<Task>>& reply_to_;
  std::shared_mutex& mutex_;

  bool dead_ { false };

public:
  Remote( EventLoop& events,
          EventCategories categories,
          TCPSocket socket,
          size_t index,
          MessageQueue& msg_q,
          Runtime& runtime,
          absl::flat_hash_map<Task, size_t, absl::Hash<Task>>& reply_to,
          std::shared_mutex& mutex );

  std::optional<Handle> start( Task&& task ) override;
  std::optional<ITaskRunner::Info> get_info() override { return info_; }

  void push_message( OutgoingMessage&& msg );

  Address local_address() { return socket_.local_address(); }
  Address peer_address() { return socket_.peer_address(); }

  void send_object( Handle handle );

  std::unordered_set<Task> pending_result_ {};

  bool dead() const { return dead_; }
  ~Remote();

private:
  void load_tx_message();
  void write_to_rb();
  void read_from_rb();
  void install_rule( EventLoop::RuleHandle rule ) { installed_rules_.push_back( rule ); }
  void process_incoming_message( IncomingMessage&& msg );

  void send_blob( Blob blob );
  void send_tree( Tree tree );

  void clean_up();
};

class NetworkWorker : public IResultCache
{
private:
  EventCategories categories_ {};

  EventLoop events_ {};
  std::thread network_thread_ {};
  std::atomic<bool> should_exit_ = false;

  Channel<TCPSocket> listening_sockets_ {};
  Channel<TCPSocket> connecting_sockets_ {};

  std::size_t next_connection_id_ { 0 };
  std::unordered_map<size_t, std::shared_ptr<Remote>> connections_ {};
  std::vector<TCPSocket> server_sockets_ {};

  MessageQueue msg_q_ {};
  Runtime& runtime_;

  absl::flat_hash_map<Task, size_t, absl::Hash<Task>> reply_to_ {};
  std::shared_mutex mutex_ {};

  void run_loop();
  void process_outgoing_message( size_t remote_id, MessagePayload&& message );

public:
  NetworkWorker( Runtime& runtime )
    : runtime_( runtime ) {};

  void start() { network_thread_ = std::thread( std::bind( &NetworkWorker::run_loop, this ) ); }

  void stop()
  {
    should_exit_ = true;
    network_thread_.join();
  }

  ~NetworkWorker() {}

  Address start_server( const Address& address )
  {
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
    TCPSocket socket;
    socket.connect( address );
    connecting_sockets_ << std::move( socket );
  }

  void finish( Task&& task, Handle result ) override;
};
