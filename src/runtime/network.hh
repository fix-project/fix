#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <concurrentqueue/concurrentqueue.h>
#include <condition_variable>
#include <functional>
#include <glog/logging.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include "channel.hh"
#include "eventloop.hh"
#include "handle.hh"
#include "interface.hh"
#include "message.hh"
#include "mutex.hh"
#include "ring_buffer.hh"
#include "runtimestorage.hh"
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
  size_t data_server_ready;
};

template<typename Connection>
class NetworkWorker;

class Remote;
class DataServer;

class Remote : public IRuntime
{
  friend class NetworkWorker<Remote>;
  friend class NetworkWorker<DataServer>;
  static constexpr size_t STORAGE_SIZE = 65536;

protected:
  TCPSocket socket_;

  MessageQueue& msg_q_;
  std::optional<std::reference_wrapper<MultiWorkerRuntime>> parent_;
  size_t index_;

  RingBuffer rx_data_ { STORAGE_SIZE };
  RingBuffer tx_data_ { STORAGE_SIZE };

  MessageParser rx_messages_ {};
  std::queue<OutgoingMessage> tx_messages_ {};

  std::string current_msg_header_ {};
  std::string_view current_msg_unsent_header_ {};
  std::string_view current_msg_unsent_payload_ {};

  std::vector<EventLoop::RuleHandle> installed_rules_ {};

  std::shared_mutex mutex_ {};
  std::condition_variable_any info_cv_ {};
  std::optional<Info> info_ {};
  absl::flat_hash_set<Handle<Relation>> reply_to_ {};

  bool dead_ { false };

  using DataProposal = std::vector<std::pair<Handle<AnyDataType>, std::variant<BlobData, TreeData>>>;
  std::unique_ptr<DataProposal> incomplete_proposal_ { std::make_unique<DataProposal>() };
  size_t proposal_size_ {};
  std::queue<std::pair<std::pair<Handle<Relation>, std::optional<Handle<Object>>>, std::unique_ptr<DataProposal>>>
    proposed_proposals_ {};

  FixTable<Named, std::atomic<bool>, AbslHash> blobs_view_ { 1000000 };
  FixTable<AnyTree, std::atomic<bool>, AbslHash, handle::any_tree_equal> trees_view_ { 1000000 };
  FixTable<Relation, std::atomic<bool>, AbslHash> relations_view_ { 100000 };

public:
  Remote( EventLoop& events,
          EventCategories categories,
          TCPSocket socket,
          size_t index,
          MessageQueue& msg_q,
          std::optional<std::reference_wrapper<MultiWorkerRuntime>> parent );

  Remote( TCPSocket socket,
          size_t index,
          MessageQueue& msg_q,
          std::optional<std::reference_wrapper<MultiWorkerRuntime>> parent );

  std::optional<BlobData> get( Handle<Named> name ) override;
  std::optional<TreeData> get( Handle<AnyTree> name ) override;
  std::optional<TreeData> get_shallow( Handle<AnyTree> ) override;
  std::optional<Handle<Object>> get( Handle<Relation> name ) override;
  std::optional<Handle<AnyTree>> get_handle( Handle<AnyTree> handle ) override;

  void put( Handle<Named> name, BlobData data ) override;
  void put( Handle<AnyTree> name, TreeData data ) override;
  void put_shallow( Handle<AnyTree> name, TreeData data ) override;
  void put( Handle<Relation> name, Handle<Object> data ) override;
  void put_force( Handle<Relation> name, Handle<Object> data ) override;

  bool contains( Handle<Named> handle ) override;
  bool contains( Handle<AnyTree> handle ) override;
  bool contains_shallow( Handle<AnyTree> handle ) override;
  bool contains( Handle<Relation> handle ) override;
  std::optional<Handle<AnyTree>> contains( Handle<AnyTreeRef> handle ) override;
  bool contains( const std::string_view label ) override;
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

protected:
  void load_tx_message();
  void write_to_rb();
  void read_from_rb();
  void install_rule( EventLoop::RuleHandle rule ) { installed_rules_.push_back( rule ); }
  void process_incoming_message( IncomingMessage&& msg );

  void send_blob( BlobData blob );
  void send_tree( Handle<AnyTree>, TreeData tree );

  void clean_up();

  bool loaded( Handle<Named> handle );
  bool loaded( Handle<AnyTree> handle );
  bool loaded( Handle<Relation> handle );

  void add_to_view( Handle<Named> handle );
  void add_to_view( Handle<AnyTree> handle );
  void add_to_view( Handle<Relation> handle );
};

class DataServer : public Remote
{
  friend class NetworkWorker<DataServer>;

private:
  Channel<Handle<Named>> ready_ {};
  std::list<std::thread> threads_ {};
  void process_incoming_message( IncomingMessage&& msg );
  void run_after( std::function<void()> );

public:
  inline static size_t latency = 0;

  DataServer( EventLoop& events,
              EventCategories categories,
              TCPSocket socket,
              size_t index,
              MessageQueue& msg_q,
              std::optional<std::reference_wrapper<MultiWorkerRuntime>> parent );

  ~DataServer();
};

template<typename Connection>
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
  SharedMutex<std::unordered_map<std::string, size_t>> addresses_ {};
  std::vector<TCPSocket> server_sockets_ {};

  MessageQueue msg_q_ {};

  std::optional<std::reference_wrapper<MultiWorkerRuntime>> parent_;

  void run_loop();
  void process_outgoing_message( size_t remote_id, MessagePayload&& message );

public:
  SharedMutex<std::unordered_map<size_t, std::shared_ptr<Connection>>> connections_ {};

  NetworkWorker( std::optional<std::reference_wrapper<MultiWorkerRuntime>> parent = {} )
    : parent_( parent )
  {}

  void start() { network_thread_ = std::thread( std::bind( &NetworkWorker::run_loop, this ) ); }

  void join() { network_thread_.join(); }

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
    VLOG( 1 ) << "Connecting to " << address.to_string();
    socket.connect( address );
    connecting_sockets_.move_push( std::move( socket ) );
  }

  std::shared_ptr<IRuntime> get_remote( const Address& address )
  {
    auto address_str = address.to_string();
    addresses_.read().wait( [&] { return addresses_.read()->contains( address_str ); } );
    auto rt = connections_.read()->at( addresses_.read()->at( address_str ) );

    std::shared_lock lock( rt->mutex_ );
    rt->info_cv_.wait( lock, [&]() { return rt->info_.has_value(); } );

    return std::static_pointer_cast<IRuntime>( rt );
  }
};

template class NetworkWorker<Remote>;
template class NetworkWorker<DataServer>;
