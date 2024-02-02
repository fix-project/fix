#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

#include <glog/logging.h>

#include "eventloop.hh"
#include "handle.hh"
#include "message.hh"
#include "network.hh"
#include "object.hh"
#include "types.hh"

using namespace std;
using Opcode = Message::Opcode;

void Remote::load_tx_message()
{
  if ( not current_msg_unsent_header_.empty() or not current_msg_unsent_payload_.empty() or tx_messages_.empty() )
    throw runtime_error( "Unable to load" );

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

void Remote::send_blob( BlobData blob )
{
  push_message( { Opcode::BLOBDATA, blob } );
}

void Remote::send_tree( TreeData tree )
{
  push_message( { Opcode::TREEDATA, tree } );
}

// void Remote::send_object( Handle<Fix> handle )
//{
//   if ( not handle.try_into<Expression>().has_value() ) {
//     throw runtime_error( "Unimplemented" );
//   }
//
//   auto blob = handle.try_into<Expression>()
//     .and_then( []( auto h ) { return h.template try_into<Object>(); } )
//     .and_then( []( auto h ) { return h.template try_into<Value>(); } )
//     .and_then( []( auto h ) { return h.template try_into<Blob>(); } )
//     .and_then( []( auto h ) { return h.template try_into<Named>(); } );
//
//   if ( blob ) {
//     send_blob( storage_.get( blob.value() ) );
//     return;
//   }
//
//   auto valuetree = handle.try_into<Expression>()
//     .and_then( []( auto h ) { return h.template try_into<Object>(); } )
//     .and_then( []( auto h ) { return h.template try_into<Value>(); } )
//     .and_then( []( auto h ) { return h.template try_into<ValueTree>(); } );
//
//   if ( valuetree ) {
//     send_tree( storage_.get( valuetree.value() ) );
//     return;
//   }
//
//   auto objecttree = handle.try_into<Expression>()
//     .and_then( []( auto h ) { return h.template try_into<Object>(); } )
//     .and_then( []( auto h ) { return h.template try_into<ObjectTree>(); } );
//
//   if ( objecttree ) {
//     send_tree( storage_.get( objecttree.value() ) );
//     return;
//   }
// }

void Remote::push_message( OutgoingMessage&& msg )
{
  VLOG( 1 ) << "push_message " << Message::OPCODE_NAMES[static_cast<uint8_t>( msg.opcode() )];
  tx_messages_.push( move( msg ) );

  if ( current_msg_unsent_header_.empty() and current_msg_unsent_payload_.empty() ) {
    load_tx_message();
  }
}

optional<BlobData> Remote::get( Handle<Named> name )
{
  RequestBlobPayload payload { .handle = name };
  msg_q_.enqueue( make_pair( index_, move( payload ) ) );

  return {};
}

optional<TreeData> Remote::get( Handle<AnyTree> name )
{
  RequestTreePayload payload { .handle = name };
  msg_q_.enqueue( make_pair( index_, move( payload ) ) );

  return {};
}

optional<Handle<Object>> Remote::get( Handle<Relation> name )
{
  VLOG( 1 ) << "Pushing runpayload " << name;
  RunPayload payload { .task = name };
  msg_q_.enqueue( make_pair( index_, move( payload ) ) );

  return {};
}

void Remote::put( __attribute__( ( unused ) ) Handle<Named> name, BlobData data )
{
  send_blob( data );
}

void Remote::put( __attribute__( ( unused ) ) Handle<AnyTree> name, TreeData data )
{
  send_tree( data );
}

void Remote::put( Handle<Relation> name, Handle<Object> data )
{
  shared_lock lock( mutex_ );
  if ( reply_to_.contains( name ) ) {
    ResultPayload payload { .task = name, .result = data };
    msg_q_.enqueue( make_pair( index_, move( payload ) ) );
  }
}

bool Remote::contains( __attribute__( ( unused ) ) Handle<Named> handle )
{
  // TODO
  return false;
}

bool Remote::contains( __attribute__( ( unused ) ) Handle<AnyTree> handle )
{
  // TODO
  return false;
}

bool Remote::contains( __attribute__( ( unused ) ) Handle<Relation> handle )
{
  // TODO
  return false;
}

bool Remote::contains( __attribute__( ( unused ) ) const std::string_view label )
{
  // TODO
  return false;
}

std::optional<IRuntime::Info> Remote::get_info()
{
  shared_lock lock( mutex_ );
  return info_;
}

Remote::Remote( EventLoop& events,
                EventCategories categories,
                TCPSocket socket,
                size_t index,
                MessageQueue& msg_q,
                weak_ptr<IRuntime> parent )
  : socket_( move( socket ) )
  , msg_q_( msg_q )
  , parent_( parent )
  , index_( index )
{
  install_rule( events.add_rule(
    categories.rx_read_data,
    socket_,
    Direction::In,
    [&] { rx_data_.push_from_fd( socket_ ); },
    [&] { return rx_data_.can_write(); },
    [&] { this->clean_up(); } ) );

  install_rule( events.add_rule(
    categories.tx_write_data,
    socket_,
    Direction::Out,
    [&] { tx_data_.pop_to_fd( socket_ ); },
    [&] { return tx_data_.can_read(); },
    [&] { this->clean_up(); } ) );

  install_rule( events.add_rule(
    categories.rx_parse_msg, [&] { read_from_rb(); }, [&] { return rx_data_.can_read(); } ) );

  install_rule( events.add_rule(
    categories.tx_serialize_msg,
    [&] { write_to_rb(); },
    [&] { return not tx_messages_.empty() and tx_data_.can_write(); } ) );

  install_rule( events.add_rule(
    categories.rx_process_msg,
    [&] {
      IncomingMessage message = move( rx_messages_.front() );
      rx_messages_.pop();
      process_incoming_message( move( message ) );
    },
    [&] { return not rx_messages_.empty(); } ) );

  push_message( { Opcode::REQUESTINFO, string( "" ) } );
}

void Remote::process_incoming_message( IncomingMessage&& msg )
{
  VLOG( 1 ) << "process_incoming_message " << Message::OPCODE_NAMES[static_cast<uint8_t>( msg.opcode() )];
  switch ( msg.opcode() ) {
    case Opcode::RUN: {
      auto payload = parse<RunPayload>( std::get<string>( msg.payload() ) );
      auto task = payload.task;
      {
        unique_lock lock( mutex_ );
        reply_to_.insert( task );
      }

      auto res = parent_.lock()->get( payload.task );
      if ( res.has_value() ) {
        bool need_send = erase_reply_to( task );

        if ( need_send ) {
          ResultPayload payload { .task = task, .result = *res };
          push_message( OutgoingMessage::to_message( move( payload ) ) );
        }
      }
      break;
    }

    case Opcode::RESULT: {
      auto payload = parse<ResultPayload>( std::get<string>( msg.payload() ) );
      pending_result_.erase( payload.task );
      parent_.lock()->put( payload.task, payload.result );
      break;
    }

    case Opcode::REQUESTINFO: {
      InfoPayload payload {
        parent_.lock()->get_info().value_or( IRuntime::Info { .parallelism = 0, .link_speed = 0 } ) };
      push_message( OutgoingMessage::to_message( move( payload ) ) );
      break;
    }

    case Opcode::INFO: {
      unique_lock lock( mutex_ );
      info_ = parse<InfoPayload>( std::get<string>( msg.payload() ) );
      break;
    }

    case Opcode::REQUESTTREE: {
      auto payload = parse<RequestTreePayload>( std::get<string>( msg.payload() ) );
      auto tree = parent_.lock()->get( payload.handle );
      if ( tree )
        send_tree( *tree );
      break;
    }

    case Opcode::REQUESTBLOB: {
      auto payload = parse<RequestBlobPayload>( std::get<string>( msg.payload() ) );
      auto blob = parent_.lock()->get( payload.handle );
      if ( blob )
        send_blob( blob.value() );
      break;
    }

    case Opcode::BLOBDATA: {
      parent_.lock()->create( msg.get_blob() );
      break;
    }

    case Opcode::TREEDATA: {
      parent_.lock()->create( msg.get_tree() );
      break;
    }

    case Opcode::PROPOSE_TRANSFER: {
      // auto [todo, result, handles] = parse<ProposeTransferPayload>( std::get<string>( msg.payload() ) );

      // size_t original = handles.size();
      // for ( auto it = handles.begin(); it != handles.end(); ) {
      //   if ( parent_.lock()->contains( *it ) ) {
      //     it = handles.erase( it );
      //   } else {
      //     ++it;
      //   }
      // }
      // VLOG( 2 ) << "Accepting " << handles.size() << " of " << original << " objects.";
      // for ( const auto& h : handles ) {
      //   VLOG( 3 ) << "Accepting " << h;
      // }
      // push_message( OutgoingMessage::to_message( AcceptTransferPayload { todo, result, handles } ) );
      throw runtime_error( "Unimplemented" );
      break;
    }

    case Opcode::ACCEPT_TRANSFER: {
      // auto [todo, result, handles] = parse<AcceptTransferPayload>( std::get<string>( msg.payload() ) );

      // VLOG( 2 ) << "Sending " << handles.size() << " objects.";
      // for ( const auto& h : handles ) {
      //   VLOG( 3 ) << "Sending " << h;
      //   send_object( h );
      // }
      // if ( result ) {
      //   push_message( OutgoingMessage::to_message( ResultPayload { .task = todo, .result = *result } ) );
      // } else {
      //   push_message( OutgoingMessage::to_message( RunPayload { .task = todo } ) );
      // }
      throw runtime_error( "Unimplemented" );
      break;
    }

    default:
      throw runtime_error( "Invalid message Opcode" );
  }

  return;
}

void Remote::clean_up()
{
  // Reset info
  info_.reset();
  dead_ = true;
}

Remote::~Remote()
{
  socket_.close();

  // Cancel rules
  for ( auto& handle : installed_rules_ ) {
    handle.cancel();
  }

  // Forward pending tasks
  for ( auto it = pending_result_.begin(); it != pending_result_.end(); ) {
    auto task = pending_result_.extract( it++ );
    parent_.lock()->get( task.value() );
  }
}

void NetworkWorker::process_outgoing_message( size_t remote_idx, MessagePayload&& payload )
{
  if ( !connections_.read()->contains( remote_idx ) ) {
    // Forward back any run
    // if ( holds_alternative<ProposeTransferPayload>( payload ) ) {
    //  auto proposed_payload = get<ProposeTransferPayload>( payload );
    //  parent_.lock()->get( move( proposed_payload.todo ) );
    //}
    if ( holds_alternative<RunPayload>( payload ) ) {
      parent_.lock()->get( move( std::get<RunPayload>( payload ).task ) );
    }
  } else {
    Remote& connection = *connections_.read()->at( remote_idx );

    bool need_send = false;

    if ( holds_alternative<ProposeTransferPayload>( payload ) ) {
      // const auto& proposed_payload = get<ProposeTransferPayload>( payload );
      // if ( !proposed_payload.result.has_value() ) {
      //   connection.pending_result_.insert( proposed_payload.todo );
      // }
      // need_send = true;
      throw std::runtime_error( "Unimplemented" );
    } else if ( holds_alternative<RunPayload>( payload ) ) {
      connection.pending_result_.insert( std::get<RunPayload>( payload ).task );
      need_send = true;
    } else if ( holds_alternative<ResultPayload>( payload ) ) {
      const auto& result_payload = std::get<ResultPayload>( payload );
      need_send = connection.erase_reply_to( result_payload.task );
    } else {
      need_send = true;
    }

    if ( need_send ) {
      connection.push_message( OutgoingMessage::to_message( move( payload ) ) );
    }
  }
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

      VLOG( 1 ) << "Listening on " << server_socket.local_address();
      // When someone connects to the socket, accept it and add the new connection to the event loop
      events_.add_rule( categories_.server_new_connection, server_socket, Direction::In, [&] {
        connections_.write()->emplace(
          next_connection_id_,
          make_unique<Remote>(
            events_, categories_, server_socket.accept(), next_connection_id_, msg_q_, parent_ ) );

        {
          addresses_.write()->emplace( connections_.read()->at( next_connection_id_ )->peer_address().to_string(),
                                       next_connection_id_ );
        }

        VLOG( 1 ) << "New connection from "
                  << connections_.read()->at( next_connection_id_ )->peer_address().to_string();

        next_connection_id_++;
      } );
    },
    [&] { return listening_sockets_.size_approx() > 0; } );

  // When we've made a new outgoing connection, add it to the event loop
  events_.add_rule(
    categories_.client_new_connection,
    [&] {
      TCPSocket client_socket = *connecting_sockets_.pop();
      connections_.write()->emplace(
        next_connection_id_,
        make_unique<Remote>(
          events_, categories_, std::move( client_socket ), next_connection_id_, msg_q_, parent_ ) );

      addresses_.write()->emplace( connections_.read()->at( next_connection_id_ )->peer_address().to_string(),
                                   next_connection_id_ );

      VLOG( 1 ) << "New connection to "
                << connections_.read()->at( next_connection_id_ )->peer_address().to_string();
      next_connection_id_++;
    },
    [&] { return connecting_sockets_.size_approx() > 0; } );

  // Forward msg_q_ to Remotes
  events_.add_rule(
    categories_.forward_msg,
    [&] {
      std::pair<uint32_t, MessagePayload> entry;
      while ( msg_q_.try_dequeue( entry ) ) {
        process_outgoing_message( entry.first, move( entry.second ) );
      }
    },
    [&] { return msg_q_.size_approx() > 0; } );

  // TODO: kick
  while ( not should_exit_ ) {
    std::erase_if( connections_.write().get(), []( const auto& item ) {
      auto const& [_, value] = item;
      return value->dead();
    } );
    events_.wait_next_event( 1 );
  }
}
