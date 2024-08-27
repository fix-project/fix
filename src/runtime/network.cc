#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdatomic.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

#include <glog/logging.h>

#include "eventloop.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "handle_util.hh"
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

void Remote::send_tree( Handle<AnyTree> handle, TreeData )
{
  parent_.value().get().visit_minrepo( handle::upcast( handle ), [&]( Handle<AnyDataType> h ) {
    h.visit<void>( overload {
      []( Handle<Literal> ) {},
      []( Handle<Relation> ) {},
      [&]( Handle<AnyTree> t ) { push_message( { Opcode::TREEDATA, parent_.value().get().get( t ).value() } ); },
      [&]( Handle<Named> b ) { push_message( { Opcode::BLOBDATA, parent_.value().get().get( b ).value() } ); },
    } );
  } );

  add_to_view( handle );
}

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

optional<TreeData> Remote::get_shallow( Handle<AnyTree> name )
{
  RequestShallowTreePayload payload { .handle = name };
  msg_q_.enqueue( make_pair( index_, move( payload ) ) );

  return {};
}

optional<Handle<Object>> Remote::get( Handle<Relation> name )
{
  if ( !contains( name ) ) {
    // XXX
    parent_.value().get().visit_minrepo( job::get_root( name ), [&]( Handle<AnyDataType> h ) {
      h.visit<void>( overload {
        []( Handle<Literal> ) {},
        []( Handle<Relation> ) {},
        [&]( auto x ) {
          if ( !loaded( x ) ) {
            msg_q_.enqueue( make_pair( index_, make_pair( x, parent_.value().get().get( x ).value() ) ) );
          }
        },
      } );
    } );
  }

  RunPayload payload { .task = name };
  msg_q_.enqueue( make_pair( index_, move( payload ) ) );

  return {};
}

void Remote::put( Handle<Named> name, BlobData data )
{
  if ( !loaded( name ) ) {
    msg_q_.enqueue( make_pair( index_, make_pair( name, data ) ) );
  }
}

void Remote::put( Handle<AnyTree> name, TreeData )
{
  if ( !loaded( name ) ) {
    parent_.value().get().visit_minrepo( handle::upcast( name ), [&]( Handle<AnyDataType> h ) {
      h.visit<void>( overload {
        []( Handle<Literal> ) {},
        []( Handle<Relation> ) {},
        [&]( auto x ) {
          if ( !loaded( x ) ) {
            msg_q_.enqueue( make_pair( index_, make_pair( x, parent_.value().get().get( x ).value() ) ) );
          }
        },
      } );
    } );
  }
}

void Remote::put_shallow( Handle<AnyTree> name, TreeData data )
{
  if ( !loaded( name ) ) {
    msg_q_.enqueue( make_pair( index_, ShallowTreeDataPayload( name, data ) ) );
  }
}

void Remote::put( Handle<Relation> name, Handle<Object> data )
{
  unique_lock lock( mutex_ );
  if ( reply_to_.contains( name ) ) {
    if ( !contains( name ) ) {
      // Send the result of the relation first
      parent_.value().get().visit_minrepo( data, [&]( Handle<AnyDataType> h ) {
        h.visit<void>( overload {
          []( Handle<Literal> ) {},
          []( Handle<Relation> ) {},
          [&]( auto x ) {
            if ( !loaded( x ) ) {
              msg_q_.enqueue( make_pair( index_, make_pair( x, parent_.value().get().get( x ).value() ) ) );
            }
          },
        } );
      } );

      VLOG( 2 ) << "Putting result to remote " << name << " " << data;
      ResultPayload payload { .task = name, .result = data };
      msg_q_.enqueue( make_pair( index_, move( payload ) ) );
    }
    reply_to_.erase( name );
  }
}

void Remote::put_force( Handle<Relation> name, Handle<Object> data )
{
  if ( !contains( name ) ) {
    // Send the result of the relation first
    parent_.value().get().visit_minrepo( data, [&]( Handle<AnyDataType> h ) {
      h.visit<void>( overload {
        []( Handle<Literal> ) {},
        []( Handle<Relation> ) {},
        [&]( auto x ) {
          if ( !loaded( x ) ) {
            msg_q_.enqueue( make_pair( index_, make_pair( x, parent_.value().get().get( x ).value() ) ) );
          }
        },
      } );
    } );

    VLOG( 2 ) << "Putting result to remote " << name << " " << data;
    ResultPayload payload { .task = name, .result = data };
    msg_q_.enqueue( make_pair( index_, move( payload ) ) );
  }
  reply_to_.erase( name );
}

bool Remote::contains( Handle<Named> handle )
{
  return blobs_view_.contains( handle );
}

bool Remote::loaded( Handle<Named> handle )
{
  return blobs_view_.contains( handle ) && blobs_view_.get_ref( handle ).load( memory_order_acquire );
}

void Remote::add_to_view( Handle<Named> handle )
{
  if ( !blobs_view_.contains( handle ) ) {
    blobs_view_.insert_no_value( handle );
  }

  blobs_view_.get_ref( handle ).store( true, memory_order_release );
}

bool Remote::contains( Handle<AnyTree> handle )
{
  return trees_view_.contains( handle );
}

bool Remote::loaded( Handle<AnyTree> handle )
{
  return trees_view_.contains( handle ) && trees_view_.get_ref( handle ).load( memory_order_acquire );
}

bool Remote::contains_shallow( Handle<AnyTree> handle )
{
  return contains( handle );
}

void Remote::add_to_view( Handle<AnyTree> handle )
{
  if ( !trees_view_.contains( handle ) ) {
    trees_view_.insert_no_value( handle );
  }

  trees_view_.get_ref( handle ).store( true, memory_order_release );
}

bool Remote::contains( Handle<Relation> handle )
{
  return relations_view_.contains( handle );
}

bool Remote::loaded( Handle<Relation> handle )
{
  return relations_view_.contains( handle ) && relations_view_.get_ref( handle ).load( memory_order_acquire );
}

void Remote::add_to_view( Handle<Relation> handle )
{
  if ( !relations_view_.contains( handle ) ) {
    relations_view_.insert_no_value( handle );
  }

  relations_view_.get_ref( handle ).store( true, memory_order_release );
}

std::optional<Handle<AnyTree>> Remote::get_handle( Handle<AnyTree> handle )
{
  return trees_view_.get_handle( handle );
}

std::optional<Handle<AnyTree>> Remote::contains( Handle<AnyTreeRef> handle )
{
  auto tmp_tree = Handle<AnyTree>::forge( handle.content );
  auto entry = trees_view_.get_handle( tmp_tree );

  if ( !entry.has_value() ) {
    return {};
  }

  auto tagged = handle.visit<bool>( []( auto h ) { return h.is_tag(); } );

  if ( tagged ) {
    return entry->visit<Handle<AnyTree>>( []( auto h ) { return h.tag(); } );
  } else {
    return entry;
  }
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
                optional<reference_wrapper<MultiWorkerRuntime>> parent )
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
  if ( !parent_.has_value() )
    return;

  auto& parent = parent_.value().get();

  VLOG( 1 ) << "process_incoming_message " << Message::OPCODE_NAMES[static_cast<uint8_t>( msg.opcode() )];

  switch ( msg.opcode() ) {
    case Opcode::RUN: {
      auto payload = parse<RunPayload>( std::get<string>( msg.payload() ) );
      auto task = payload.task;
      {
        unique_lock lock( mutex_ );
        reply_to_.insert( task );
      }

      auto res = parent.get( payload.task );
      if ( res.has_value() ) {
        this->put( payload.task, res.value() );
      }
      break;
    }

    case Opcode::RESULT: {
      auto payload = parse<ResultPayload>( std::get<string>( msg.payload() ) );
      pending_result_.erase( payload.task );
      parent.put( payload.task, payload.result );
      break;
    }

    case Opcode::REQUESTINFO: {
      auto parent_info = parent.get_info().value_or( IRuntime::Info { .parallelism = 0, .link_speed = 0 } );
      InfoPayload payload {
        .parallelism = parent_info.parallelism, .link_speed = parent_info.link_speed, .data = parent.data() };
      push_message( OutgoingMessage::to_message( move( payload ) ) );
      break;
    }

    case Opcode::INFO: {
      auto payload = parse<InfoPayload>( std::get<string>( msg.payload() ) );
      {
        unique_lock lock( mutex_ );
        info_ = { .parallelism = payload.parallelism, .link_speed = payload.link_speed };
        info_cv_.notify_all();
      }

      for ( auto handle : payload.data ) {
        handle.visit<void>( overload {
          [&]( Handle<Named> h ) {
            blobs_view_.insert_no_value( h );
            blobs_view_.get_ref( h ).store( false, memory_order_release );
          },
          [&]( Handle<AnyTree> t ) {
            trees_view_.insert_no_value( t );
            trees_view_.get_ref( t ).store( false, memory_order_release );
          },
          []( Handle<Literal> ) {},
          [&]( Handle<Relation> r ) {
            relations_view_.insert_no_value( r );
            relations_view_.get_ref( r ).store( false, memory_order_release );
          },
        } );
      }

      break;
    }

    case Opcode::REQUESTTREE: {
      auto payload = parse<RequestTreePayload>( std::get<string>( msg.payload() ) );
      auto tree = parent.get( payload.handle );
      if ( tree && !trees_view_.contains( payload.handle ) ) {
        send_tree( payload.handle, tree.value() );
        add_to_view( payload.handle );
      }
      break;
    }

    case Opcode::REQUESTBLOB: {
      auto payload = parse<RequestBlobPayload>( std::get<string>( msg.payload() ) );
      auto blob = parent.get( payload.handle );
      if ( blob && !blobs_view_.contains( payload.handle ) ) {
        send_blob( blob.value() );
        add_to_view( payload.handle );
      }
      break;
    }

    case Opcode::REQUESTSHALLOWTREE: {
      auto payload = parse<RequestShallowTreePayload>( std::get<string>( msg.payload() ) );
      auto tree = parent.get_shallow( payload.handle );
      if ( tree ) {
        push_message( { Opcode::SHALLOWTREEDATA, tree.value() } );
      }
      break;
    }

    case Opcode::BLOBDATA: {
      parent.create( msg.get_blob() );
      break;
    }

    case Opcode::TREEDATA: {
      parent.create( msg.get_tree() );
      break;
    }

    case Opcode::SHALLOWTREEDATA: {
      auto payload = parse<ShallowTreeDataPayload>( std::get<string>( msg.payload() ) );
      parent.put_shallow( payload.handle, payload.data );
      break;
    }

    case Opcode::PROPOSE_TRANSFER: {
      auto [todo, result, handles] = parse<ProposeTransferPayload>( std::get<string>( msg.payload() ) );

      size_t original = handles.size();

      for ( auto it = handles.begin(); it != handles.end(); ) {
        // Any handle proposed by the remote are considered "existing" on remote
        auto contained = std::visit( overload {
                                       [&]( Handle<Named> h ) {
                                         add_to_view( h );

                                         if ( parent.contains( h ) ) {
                                           parent.get( h );
                                         }
                                         return parent.contains( h );
                                       },
                                       [&]( Handle<AnyTree> t ) {
                                         add_to_view( t );

                                         if ( parent.contains( t ) ) {
                                           parent.get( t );
                                         }
                                         return parent.contains( t );
                                       },
                                       []( Handle<Literal> ) { return true; },
                                       []( Handle<Relation> ) { return true; },
                                     },
                                     //[&]( auto ) -> bool { VLOG( 1 ) << handle::fix( *it );
                                     // throw std::runtime_error( "Invalid propose transfer payload." ); } },
                                     it->get() );

        if ( contained ) {
          it = handles.erase( it );
        } else {
          ++it;
        }
      }

      VLOG( 1 ) << "Accepting " << handles.size() << " of " << original << " objects.";
      for ( const auto& h : handles ) {
        VLOG( 2 ) << "Accepting " << h;
      }
      push_message( OutgoingMessage::to_message( AcceptTransferPayload { todo, result, handles } ) );
      break;
    }

    case Opcode::ACCEPT_TRANSFER: {
      auto [todo, result, handles] = parse<AcceptTransferPayload>( std::get<string>( msg.payload() ) );

      if ( proposed_proposals_.size() == 0 ) {
        throw std::runtime_error( "Mismatch propose and accept" );
      }

      VLOG( 1 ) << "Sending " << handles.size() << " objects for " << todo;
      for ( const auto& h : handles ) {
        VLOG( 2 ) << "Sending " << handle::fix( h );
        std::visit( overload {
                      [&]( Handle<Named> n ) {
                        if ( !contains( n ) ) {
                          push_message( { Opcode::BLOBDATA,
                                          std::get<BlobData>( proposed_proposals_.front().second->at( h ) ) } );
                        }
                      },
                      [&]( Handle<AnyTree> t ) {
                        if ( !contains( t ) ) {
                          push_message( { Opcode::TREEDATA,
                                          std::get<TreeData>( proposed_proposals_.front().second->at( h ) ) } );
                        }
                      },
                      []( Handle<Literal> ) {},
                      []( Handle<Relation> ) {},
                    },
                    h.get() );
      }

      // Any objects in this proposal are considered "exising" on the remote side
      for ( const auto& [h, _] : *proposed_proposals_.front().second ) {
        std::visit( overload {
                      [&]( Handle<Named> h ) { add_to_view( h ); },
                      [&]( Handle<AnyTree> t ) { add_to_view( t ); },
                      []( Handle<Relation> ) {},
                      []( Handle<Literal> ) {},
                    },
                    h.get() );
      }

      proposed_proposals_.pop();

      if ( result ) {
        push_message( OutgoingMessage::to_message( ResultPayload { .task = todo, .result = *result } ) );
        add_to_view( todo );
      } else {
        push_message( OutgoingMessage::to_message( RunPayload { .task = todo } ) );
      }

      while ( !proposed_proposals_.empty() ) {
        // Handle and Run/Result payload without corresponding proposal but should be started now
        if ( proposed_proposals_.front().second->empty() ) {
          auto pending_todo = proposed_proposals_.front().first.first;
          auto pending_result = proposed_proposals_.front().first.second;
          if ( pending_result ) {
            push_message(
              OutgoingMessage::to_message( ResultPayload { .task = pending_todo, .result = *pending_result } ) );
            add_to_view( pending_todo );
          } else {
            push_message( OutgoingMessage::to_message( RunPayload { .task = pending_todo } ) );
          }

          proposed_proposals_.pop();
        } else {
          break;
        }
      }
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
  if ( parent_.has_value() ) {
    for ( auto it = pending_result_.begin(); it != pending_result_.end(); ) {
      auto task = pending_result_.extract( it++ );
      parent_->get().get( task.value() );
    }
  }
}

void NetworkWorker::process_outgoing_message( size_t remote_idx, MessagePayload&& payload )
{
  if ( !connections_.read()->contains( remote_idx ) ) {
    if ( holds_alternative<RunPayload>( payload ) ) {
      if ( !parent_.has_value() )
        return;

      parent_->get().get( move( std::get<RunPayload>( payload ).task ) );
    }
  } else {
    Remote& connection = *connections_.read()->at( remote_idx );

    visit(
      overload {
        [&]( BlobDataPayload b ) {
          if ( !connection.loaded( b.first ) ) {
            VLOG( 2 ) << "Adding " << b.first << " to proposal";
            connection.incomplete_proposal_->emplace( b.first, b.second );
            connection.proposal_size_ += b.second->size();
          }
        },
        [&]( TreeDataPayload t ) {
          if ( !connection.loaded( t.first ) ) {
            VLOG( 2 ) << "Adding " << t.first << " to proposal";
            connection.incomplete_proposal_->emplace(
              visit( []( auto h ) -> Handle<AnyDataType> { return h; }, t.first.get() ), t.second );
            connection.proposal_size_ += t.second->size() * sizeof( Handle<Fix> );
          }
        },
        [&]( RunPayload r ) {
          if ( connection.incomplete_proposal_->empty() && connection.proposed_proposals_.empty() ) {
            VLOG( 2 ) << "No proposal sending run directly";
            connection.push_message( OutgoingMessage::to_message( r ) );
          } else if ( connection.incomplete_proposal_->empty() ) {
            // Payload should be sent after last proposed_proposals_ is sent
            connection.proposed_proposals_.push( { pair<Handle<Relation>, optional<Handle<Object>>> { r.task, {} },
                                                   make_unique<Remote::DataProposal>() } );
          } else if ( connection.proposal_size_ < 1048576 ) {
            // Proposal too small, sending directly
            for ( const auto& [name, data] : *connection.incomplete_proposal_ ) {
              auto h = name;
              h.visit<void>( overload {
                [&]( Handle<Named> ) {
                  connection.push_message( { Opcode::BLOBDATA, std::get<BlobData>( data ) } );
                },
                [&]( Handle<AnyTree> ) {
                  connection.push_message( { Opcode::TREEDATA, std::get<TreeData>( data ) } );
                },
                []( Handle<Literal> ) {},
                []( Handle<Relation> ) {},
              } );
            }
            connection.push_message( OutgoingMessage::to_message( r ) );
            connection.incomplete_proposal_ = make_unique<Remote::DataProposal>();
            connection.proposal_size_ = 0;
          } else {
            ProposeTransferPayload payload;
            payload.todo = r.task;
            for ( const auto& [name, _] : *connection.incomplete_proposal_ ) {
              payload.handles.push_back( name );
            }
            connection.push_message( OutgoingMessage::to_message( move( payload ) ) );
            connection.proposed_proposals_.push( { pair<Handle<Relation>, optional<Handle<Object>>> { r.task, {} },
                                                   std::move( connection.incomplete_proposal_ ) } );
            connection.incomplete_proposal_ = make_unique<Remote::DataProposal>();
            connection.proposal_size_ = 0;
          }

          connection.pending_result_.insert( r.task );
        },
        [&]( ResultPayload r ) {
          if ( connection.incomplete_proposal_->empty() && connection.proposed_proposals_.empty() ) {
            VLOG( 2 ) << "No proposal sending result directly";
            connection.push_message( OutgoingMessage::to_message( r ) );
            connection.add_to_view( r.task );
          } else if ( connection.incomplete_proposal_->empty() ) {
            // Paylod should be send after last proposed_proposals_ is sent
            connection.proposed_proposals_.push(
              { pair<Handle<Relation>, optional<Handle<Object>>> { r.task, r.result },
                make_unique<Remote::DataProposal>() } );
          } else if ( connection.proposal_size_ < 1048576 ) {
            // Proposal too small, sending directly
            for ( const auto& [name, data] : *connection.incomplete_proposal_ ) {
              auto h = name;
              h.visit<void>( overload {
                [&]( Handle<Named> ) {
                  connection.push_message( { Opcode::BLOBDATA, std::get<BlobData>( data ) } );
                },
                [&]( Handle<AnyTree> ) {
                  connection.push_message( { Opcode::TREEDATA, std::get<TreeData>( data ) } );
                },
                []( Handle<Literal> ) {},
                []( Handle<Relation> ) {},
              } );
            }
            connection.push_message( OutgoingMessage::to_message( r ) );
            connection.incomplete_proposal_ = make_unique<Remote::DataProposal>();
            connection.proposal_size_ = 0;
          } else {
            ProposeTransferPayload payload;
            payload.todo = r.task;
            payload.result = r.result;
            for ( const auto& [name, _] : *connection.incomplete_proposal_ ) {
              VLOG( 2 ) << "Proposing " << name << " for " << r.task;
              payload.handles.push_back( name );
            }
            connection.push_message( OutgoingMessage::to_message( move( payload ) ) );
            connection.proposed_proposals_.push(
              { pair<Handle<Relation>, optional<Handle<Object>>> { r.task, r.result },
                std::move( connection.incomplete_proposal_ ) } );
            connection.incomplete_proposal_ = make_unique<Remote::DataProposal>();
            connection.proposal_size_ = 0;
          }
        },
        [&]( auto&& payload ) { connection.push_message( OutgoingMessage::to_message( move( payload ) ) ); } },
      payload );
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

        if ( parent_.has_value() ) {
          parent_.value().get().add_worker( connections_.read()->at( next_connection_id_ ) );
        }

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

      if ( parent_.has_value() ) {
        parent_.value().get().add_worker( connections_.read()->at( next_connection_id_ ) );
      }

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
