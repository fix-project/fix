#include "network.hh"
#include "handle.hh"
#include "message.hh"
#include "runtime.hh"
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <utility>

using namespace std;
using Opcode = Message::Opcode;

void Remote::load_tx_message()
{
  if ( not current_msg_unsent_header_.empty() or not current_msg_unsent_payload_.empty() or tx_messages_.empty() )
    throw runtime_error( "Unable to load" );

  std::cout << "loading tx " << Message::OPCODE_NAMES[static_cast<uint8_t>( tx_messages_.front().opcode() )]
            << std::endl;
  tx_messages_.front().serialize_header( current_msg_header_ );
  current_msg_unsent_header_ = current_msg_header_;
  current_msg_unsent_payload_ = tx_messages_.front().payload();
}

void Remote::write_to_rb()
{
  if ( not current_msg_unsent_header_.empty() ) {
    current_msg_unsent_header_.remove_prefix( tx_data_.push_from_const_str( current_msg_unsent_header_ ) );
  } else if ( not current_msg_unsent_payload_.empty() ) {
    current_msg_unsent_payload_.remove_prefix( tx_data_.push_from_const_str( current_msg_unsent_payload_ ) );
  } else {
    tx_messages_.pop();

    if ( not tx_messages_.empty() ) {
      load_tx_message();
    }
  }
}

void Remote::read_from_rb()
{
  rx_data_.pop( rx_messages_.parse( rx_data_.readable_region() ) );
}

void Remote::send_blob( string_view blob )
{
  push_message( { Opcode::BLOBDATA, blob } );
}

void Remote::send_tree( span_view<Handle> tree )
{
  push_message(
    { Opcode::TREEDATA, string_view( reinterpret_cast<const char*>( tree.data() ), tree.byte_size() ) } );
}

void Remote::send_tag( span_view<Handle> tag )
{
  push_message( { Opcode::TAGDATA, string_view( reinterpret_cast<const char*>( tag.data() ), tag.byte_size() ) } );
}

void Remote::send_object( Handle handle )
{
  if ( !handle.is_literal_blob() ) {
    switch ( handle.get_content_type() ) {
      case ContentType::Blob: {
        send_blob( runtime_.storage().get_blob( handle ) );
        break;
      }

      case ContentType::Tree: {
        send_tree( runtime_.storage().get_tree( handle ) );
        break;
      }

      case ContentType::Tag: {
        send_tag( runtime_.storage().get_tree( handle ) );
        break;
      }

      default:
        break;
    }
  }
}

void Remote::push_message( Message&& msg )
{
  tx_messages_.push( std::move( msg ) );

  if ( current_msg_unsent_header_.empty() and current_msg_unsent_payload_.empty() ) {
    load_tx_message();
  }
}

Remote::Remote( EventLoop& events,
                EventCategories categories,
                TCPSocket socket,
                size_t index,
                MessageQueue& msg_q,
                Runtime& runtime,
                absl::flat_hash_map<Task, size_t, absl::Hash<Task>>& reply_to,
                shared_mutex& mutex )
  : events_( events )
  , categories_( categories )
  , socket_( move( socket ) )
  , index_( index )
  , msg_q_( msg_q )
  , reply_to_( reply_to )
  , mutex_( mutex )
  , runtime_( runtime )
{
  install_rule( events_.add_rule(
    categories.rx_read_data,
    socket_,
    Direction::In,
    [&] { rx_data_.push_from_fd( socket_ ); },
    [&] { return rx_data_.can_write(); },
    [&] {
      // TODO: cleanup
    } ) );

  install_rule( events_.add_rule(
    categories.tx_write_data, [&] { tx_data_.pop_to_fd( socket_ ); }, [&] { return tx_data_.can_read(); } ) );

  install_rule( events_.add_rule(
    categories.rx_parse_msg, [&] { read_from_rb(); }, [&] { return rx_data_.can_read(); } ) );

  install_rule( events_.add_rule(
    categories.tx_serialize_msg, [&] { write_to_rb(); }, [&] { return not tx_messages_.empty(); } ) );

  install_rule( events_.add_rule(
    categories.rx_process_msg,
    [&] {
      Message message = std::move( rx_messages_.front() );
      rx_messages_.pop();
      process_incoming_message( std::move( message ) );
    },
    [&] { return not rx_messages_.empty(); } ) );

  push_message( { Opcode::REQUESTINFO, string( "" ) } );
}

std::optional<Handle> Remote::start( Task&& task )
{
  RunPayload payload { .task = std::move( task ) };
  msg_q_.enqueue( make_pair( index_, Message( Opcode::RUN, serialize( payload ) ) ) );
  return {};
}

void Remote::process_incoming_message( Message&& msg )
{
  std::cout << "process_incoming_message " << Message::OPCODE_NAMES[static_cast<uint8_t>( msg.opcode() )]
            << std::endl;
  switch ( msg.opcode() ) {
    case Opcode::RUN: {
      auto payload = parse<RunPayload>( msg.payload() );
      auto task = payload.task;
      {
        std::unique_lock lock( mutex_ );
        reply_to_.insert( { task, index_ } );
      }

      auto res = runtime_.start( std::move( payload.task ) );

      if ( res.has_value() ) {
        {
          std::unique_lock lock( mutex_ );
          if ( reply_to_.contains( task ) ) {
            reply_to_.erase( task );
            ResultPayload payload { .task = task, .result = res.value() };
            push_message( { Opcode::RESULT, serialize( payload ) } );
          }
        }
      }
      break;
    }

    case Opcode::RESULT: {
      auto payload = parse<ResultPayload>( msg.payload() );
      runtime_.finish( std::move( payload.task ), payload.result );
      break;
    }

    case Opcode::REQUESTINFO: {
      InfoPayload payload { { .parallelism = thread::hardware_concurrency() } };
      push_message( { Opcode::INFO, serialize( payload ) } );
      break;
    }

    case Opcode::INFO: {
      info_ = parse<InfoPayload>( msg.payload() );
      break;
    }

    case Opcode::BLOBDATA: {
      runtime_.storage().add_canonical_blob( move( msg.get_blob() ) );
      break;
    }

    case Opcode::TREEDATA: {
      runtime_.storage().add_canonical_tree( move( msg.get_tree() ) );
      break;
    }

    case Opcode::TAGDATA: {
      runtime_.storage().add_canonical_tag( move( msg.get_tree() ) );
      break;
    }

    default:
      throw runtime_error( "Invalid message Opcode" );
  }

  return;
}

void NetworkWorker::run_loop()
{
  categories_ = {
    .server_new_socket = events_.add_category( "server - new socket" ),
    .server_new_connection = events_.add_category( "server - new connection" ),
    .client_new_connection = events_.add_category( "client - new connection" ),
    .rx_read_data = events_.add_category( "rx - read" ),
    .rx_parse_msg = events_.add_category( "rx - parse message" ),
    .rx_process_msg = events_.add_category( "rx - process message" ),
    .tx_serialize_msg = events_.add_category( "tx - serialize message" ),
    .tx_write_data = events_.add_category( "tx - write" ),
    .forward_msg = events_.add_category( "networkworker - forward msg to remote" ),
  };
  // When we have a new server socket, add it to the event loop
  events_.add_rule(
    categories_.server_new_socket,
    [&] {
      server_sockets_.push_back( *listening_sockets_.pop() );
      TCPSocket& server_socket = server_sockets_.back();
      // When someone connects to the socket, accept it and add the new connection to the event loop
      events_.add_rule( categories_.server_new_connection, server_socket, Direction::In, [&] {
        connections_.emplace_back(
          events_, categories_, server_socket.accept(), connections_.size(), msg_q_, runtime_, reply_to_, mutex_ );
        runtime_.add_task_runner( connections_.back() );
      } );
    },
    [&] { return listening_sockets_.size_approx() > 0; } );

  // When we've made a new outgoing connection, add it to the event loop
  events_.add_rule(
    categories_.client_new_connection,
    [&] {
      TCPSocket client_socket = *connecting_sockets_.pop();
      connections_.emplace_back( events_,
                                 categories_,
                                 std::move( client_socket ),
                                 connections_.size(),
                                 msg_q_,
                                 runtime_,
                                 reply_to_,
                                 mutex_ );
      runtime_.add_task_runner( connections_.back() );
    },
    [&] { return connecting_sockets_.size_approx() > 0; } );

  // Forward msg_q_ to Remotes
  events_.add_rule(
    categories_.forward_msg,
    [&] {
      std::pair<uint32_t, Message> entry;
      while ( msg_q_.try_dequeue( entry ) ) {
        assert( entry.first < connections_.size() );
        connections_.at( entry.first ).push_message( std::move( entry.second ) );
      }
    },
    [&] { return msg_q_.size_approx() > 0; } );

  // TODO: kick
  while ( not should_exit_ ) {
    events_.wait_next_event( 1 );
  }
}

void NetworkWorker::finish( Task&& task, Handle result )
{
  std::shared_lock lock( mutex_ );
  if ( reply_to_.contains( task ) ) {
    ResultPayload payload { .task = std::move( task ), .result = result };
    msg_q_.enqueue( make_pair( reply_to_.at( task ), Message( Opcode::RESULT, serialize( payload ) ) ) );
  }
}
