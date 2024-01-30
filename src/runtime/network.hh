#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <concurrentqueue/concurrentqueue.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "channel.hh"
#include "eventloop.hh"
#include "handle.hh"
#include "interface.hh"
#include "message.hh"
#include "ring_buffer.hh"
#include "socket.hh"

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

class Remote : public IRuntime
{
  TCPSocket socket_;

  MessageQueue& msg_q_;
  std::weak_ptr<IRuntime> parent_;
  size_t index_;

  RingBuffer rx_data_ { 8192 };
  RingBuffer tx_data_ { 8192 };

  MessageParser rx_messages_ {};
  std::queue<OutgoingMessage> tx_messages_ {};

  std::string current_msg_header_ {};
  std::string_view current_msg_unsent_header_ {};
  std::string_view current_msg_unsent_payload_ {};

  std::vector<EventLoop::RuleHandle> installed_rules_ {};

  std::shared_mutex mutex_ {};
  std::optional<Info> info_ {};
  absl::flat_hash_set<Handle<Relation>> reply_to_ {};

  bool dead_ { false };

public:
  Remote( EventLoop& events,
          EventCategories categories,
          TCPSocket socket,
          size_t index,
          MessageQueue& msg_q,
          std::weak_ptr<IRuntime> parent );

  std::optional<BlobData> get( Handle<Named> name ) override;
  std::optional<TreeData> get( Handle<AnyTree> name ) override;
  std::optional<Handle<Object>> get( Handle<Relation> name ) override;

  void put( Handle<Named> name, BlobData data ) override;
  void put( Handle<AnyTree> name, TreeData data ) override;
  void put( Handle<Relation> name, Handle<Object> data ) override;

  bool contains( Handle<Named> handle ) override;
  bool contains( Handle<AnyTree> handle ) override;
  bool contains( Handle<Relation> handle ) override;
  std::optional<Info> get_info() override;

  void push_message( OutgoingMessage&& msg );

  Address local_address() { return socket_.local_address(); }
  Address peer_address() { return socket_.peer_address(); }

  std::unordered_set<Handle<Relation>> pending_result_ {};

  bool dead() const { return dead_; }

  bool erase_reply_to( Handle<Relation> handle )
  {
    std::unique_lock lock( mutex_ );
    return reply_to_.erase( handle );
  }
  ~Remote();

private:
  void load_tx_message();
  void write_to_rb();
  void read_from_rb();
  void install_rule( EventLoop::RuleHandle rule ) { installed_rules_.push_back( rule ); }
  void process_incoming_message( IncomingMessage&& msg );

  void send_blob( BlobData blob );
  void send_tree( TreeData tree );

  void clean_up();
};

class NetworkWorker
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

  std::weak_ptr<IRuntime> parent_;

  void run_loop();
  void process_outgoing_message( size_t remote_id, MessagePayload&& message );

public:
  NetworkWorker( std::weak_ptr<IRuntime> parent )
    : parent_( parent )
  {}

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
    listening_sockets_.move_push( std::move( socket ) );
    return listen_address;
  }

  void connect( const Address& address )
  {
    TCPSocket socket;
    socket.connect( address );
    connecting_sockets_.move_push( std::move( socket ) );
  }

  std::shared_ptr<Remote> get_remote( size_t index ) { return connections_.at( index ); }
};
