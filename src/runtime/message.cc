#include "message.hh"
#include "handle.hh"
#include "object.hh"
#include "overload.hh"
#include "parser.hh"
#include "types.hh"

#include <glog/logging.h>
#include <memory>
#include <span>

using namespace std;

IncomingMessage::IncomingMessage( const Message::Opcode opcode, string&& payload )
  : Message( opcode )
  , payload_( std::move( payload ) )
{}

IncomingMessage::IncomingMessage( const Message::Opcode opcode, OwnedMutBlob&& payload )
  : Message( opcode )
  , payload_( std::move( payload ) )
{}

IncomingMessage::IncomingMessage( const Message::Opcode opcode, OwnedMutTree&& payload )
  : Message( opcode )
  , payload_( std::move( payload ) )
{}

OutgoingMessage::OutgoingMessage( const Message::Opcode opcode, BlobData payload )
  : Message( opcode )
  , payload_( payload )
{}

OutgoingMessage::OutgoingMessage( const Message::Opcode opcode, TreeData payload )
  : Message( opcode )
  , payload_( payload )
{}

OutgoingMessage::OutgoingMessage( const Message::Opcode opcode, string&& payload )
  : Message( opcode )
  , payload_( payload )
{}

void OutgoingMessage::serialize_header( string& out )
{
  out.resize( Message::HEADER_LENGTH );
  Serializer s { out };
  s.integer( payload_length() );
  s.integer( static_cast<uint8_t>( opcode() ) );

  if ( s.bytes_written() != Message::HEADER_LENGTH ) {
    throw runtime_error( "Wrong header length" );
  }
}

string_view OutgoingMessage::payload()
{
  return std::visit( overload {
                       []( BlobData& b ) -> string_view {
                         return { b->data(), b->size() };
                       },
                       []( TreeData& t ) -> string_view {
                         return { reinterpret_cast<const char*>( t->data() ), t->span().size_bytes() };
                       },
                       []( string& s ) -> string_view { return s; },
                     },
                     payload_ );
}

size_t OutgoingMessage::payload_length()
{
  return std::visit( overload {
                       []( BlobData& b ) { return b->size(); },
                       []( TreeData& t ) { return t->span().size_bytes(); },
                       []( string& s ) { return s.size(); },
                     },
                     payload_ );
}

Message::Opcode Message::opcode( string_view header )
{
  assert( header.size() == HEADER_LENGTH );
  return static_cast<Opcode>( header[8] );
}

size_t IncomingMessage::expected_payload_length( string_view header )
{
  Parser p { header };
  size_t payload_length = 0;
  p.integer( payload_length );

  if ( p.error() ) {
    throw runtime_error( "Unable to parse header" );
  }

  return payload_length;
}

OutgoingMessage OutgoingMessage::to_message( MessagePayload&& payload )
{
  return std::visit( overload {
                       []( BlobDataPayload b ) -> OutgoingMessage { return { Opcode::BLOBDATA, b.second }; },
                       []( TreeDataPayload t ) -> OutgoingMessage { return { Opcode::TREEDATA, t.second }; },
                       []( auto&& p ) -> OutgoingMessage {
                         using T = std::decay_t<decltype( p )>;
                         return { T::OPCODE, serialize( p ) };
                       },
                     },
                     payload );
}

void MessageParser::complete_message()
{
  std::visit(
    [&]( auto&& arg ) { completed_messages_.emplace( Message::opcode( incomplete_header_ ), std::move( arg ) ); },
    incomplete_payload_ );

  expected_payload_length_.reset();
  incomplete_header_.clear();
  incomplete_payload_ = "";
  completed_payload_length_ = 0;
}

size_t MessageParser::parse( string_view buf )
{
  size_t consumed_bytes = buf.length();

  while ( not buf.empty() ) {
    if ( not expected_payload_length_.has_value() ) {
      const auto remaining_length = min( buf.length(), Message::HEADER_LENGTH - incomplete_header_.length() );
      incomplete_header_.append( buf.substr( 0, remaining_length ) );
      buf.remove_prefix( remaining_length );

      if ( incomplete_header_.length() == Message::HEADER_LENGTH ) {
        expected_payload_length_ = IncomingMessage::expected_payload_length( incomplete_header_ );

        if ( expected_payload_length_.value() == 0 ) {
          switch ( Message::opcode( incomplete_header_ ) ) {
            case Message::Opcode::TREEDATA:
              incomplete_payload_ = OwnedMutTree::allocate( 0 );
              break;

            case Message::Opcode::BLOBDATA: {
              incomplete_payload_ = OwnedMutBlob::allocate( 0 );
              break;
            }

            default:
              break;
          }
          complete_message();
        } else {
          switch ( Message::opcode( incomplete_header_ ) ) {
            case Message::Opcode::RUN:
            case Message::Opcode::INFO:
            case Message::Opcode::RESULT:
            case Message::Opcode::REQUESTTREE:
            case Message::Opcode::REQUESTBLOB:
            case Message::Opcode::LOADBLOB:
            case Message::Opcode::LOADTREE:
            case Message::Opcode::REQUESTSHALLOWTREE:
            case Message::Opcode::PROPOSE_TRANSFER:
            case Message::Opcode::ACCEPT_TRANSFER:
            case Message::Opcode::SHALLOWTREEDATA: {
              incomplete_payload_ = "";
              get<string>( incomplete_payload_ ).resize( expected_payload_length_.value() );
              break;
            }

            case Message::Opcode::BLOBDATA: {
              incomplete_payload_ = OwnedMutBlob::allocate( expected_payload_length_.value() );
              break;
            }

            case Message::Opcode::TREEDATA: {
              incomplete_payload_
                = OwnedMutTree::allocate( expected_payload_length_.value() / sizeof( Handle<Fix> ) );
              break;
            }

            default:
              throw runtime_error( "Invalid combination of message type and payload size." );
          }
        }
      }
    } else {
      std::visit(
        [&]( auto& arg ) {
          char* data = const_cast<char*>( reinterpret_cast<const char*>( arg.data() ) ) + completed_payload_length_;
          const auto remaining_length
            = min( buf.length(), expected_payload_length_.value() - completed_payload_length_ );
          memcpy( data, buf.data(), remaining_length );

          buf.remove_prefix( remaining_length );
          completed_payload_length_ += remaining_length;

          if ( completed_payload_length_ == expected_payload_length_.value() ) {
            complete_message();
          }
        },
        incomplete_payload_ );
    }
  }

  return consumed_bytes;
}

template<FixType F>
Handle<F> parse_handle( Parser& parser )
{
  u8x32 handle;
  parser.integer( handle );

  if ( parser.error() ) {
    throw runtime_error( "Failed to parse handle." );
  }

  return Handle<F>::forge( handle );
}

RunPayload RunPayload::parse( Parser& parser )
{
  return { .task { parse_handle<Relation>( parser ) } };
}

void RunPayload::serialize( Serializer& serializer ) const
{
  serializer.integer( task.content );
}

RequestBlobPayload RequestBlobPayload::parse( Parser& parser )
{
  return { .handle { parse_handle<Named>( parser ) } };
}

void RequestBlobPayload::serialize( Serializer& serializer ) const
{
  serializer.integer( handle.content );
}

RequestTreePayload RequestTreePayload::parse( Parser& parser )
{
  return { .handle { parse_handle<AnyTree>( parser ) } };
}

void RequestTreePayload::serialize( Serializer& serializer ) const
{
  serializer.integer( handle.content );
}

RequestShallowTreePayload RequestShallowTreePayload::parse( Parser& parser )
{
  return { .handle { parse_handle<AnyTree>( parser ) } };
}

void RequestShallowTreePayload::serialize( Serializer& serializer ) const
{
  serializer.integer( handle.content );
}

ResultPayload ResultPayload::parse( Parser& parser )
{
  ResultPayload res;
  res.task = parse_handle<Relation>( parser );
  res.result = parse_handle<Object>( parser );
  return res;
}

void ResultPayload::serialize( Serializer& serializer ) const
{
  serializer.integer( task.content );
  serializer.integer( result.content );
}

InfoPayload InfoPayload::parse( Parser& parser )
{
  uint32_t parallelism {};
  double link_speed {};

  parser.integer( parallelism );
  if ( parser.error() ) {
    throw runtime_error( "Failed to parse parallelism." );
  }

  parser.integer( link_speed );
  if ( parser.error() ) {
    throw runtime_error( "Failed to parse link speed." );
  }

  size_t data_entries {};
  parser.integer<size_t>( data_entries );
  if ( parser.error() ) {
    throw runtime_error( "Failed to parse number of entries in data." );
  }

  InfoPayload payload;
  payload.parallelism = parallelism;
  payload.link_speed = link_speed;
  payload.data.reserve( data_entries );

  for ( size_t i = 0; i < data_entries; i++ ) {
    payload.data.insert( parse_handle<AnyDataType>( parser ) );
  }
  return payload;
}

void InfoPayload::serialize( Serializer& serializer ) const
{
  serializer.integer( parallelism );
  serializer.integer( link_speed );
  serializer.integer( data.size() );
  for ( const auto& h : data ) {
    serializer.integer( h.content );
  }
}

ShallowTreeDataPayload ShallowTreeDataPayload::parse( Parser& parser )
{
  ShallowTreeDataPayload payload;
  payload.handle = parse_handle<AnyTree>( parser );
  auto tree = OwnedMutTree::allocate( parser.input().size() / sizeof( Handle<Fix> ) );
  parser.string( { reinterpret_cast<char*>( tree.span().data() ), tree.span().size_bytes() } );
  payload.data = make_shared<OwnedTree>( std::move( tree ) );
  return payload;
}

void ShallowTreeDataPayload::serialize( Serializer& serializer ) const
{
  serializer.integer( handle.content );
  serializer.string(
    string_view( reinterpret_cast<const char*>( data->span().data() ), data->span().size_bytes() ) );
}

LoadBlobPayload LoadBlobPayload::parse( Parser& parser )
{
  LoadBlobPayload payload;
  payload.handle = parse_handle<Blob>( parser );
  return payload;
}

void LoadBlobPayload::serialize( Serializer& serializer ) const
{
  serializer.integer( handle.content );
}

LoadTreePayload LoadTreePayload::parse( Parser& parser )
{
  LoadTreePayload payload;
  payload.handle = parse_handle<AnyTree>( parser );
  return payload;
}

void LoadTreePayload::serialize( Serializer& serializer ) const
{
  serializer.integer( handle.content );
}

template<Message::Opcode O>
TransferPayload<O> TransferPayload<O>::parse( Parser& parser )
{
  auto handle = parse_handle<Relation>( parser );
  TransferPayload payload { .todo = handle };

  bool has_result = false;
  parser.integer<bool>( has_result );

  if ( has_result )
    payload.result = parse_handle<Object>( parser );

  size_t count = 0;
  parser.integer<size_t>( count );
  payload.handles.reserve( count );
  for ( size_t i = 0; i < count; i++ ) {
    payload.handles.push_back( parse_handle<AnyDataType>( parser ) );
  }
  return payload;
}

template<Message::Opcode O>
void TransferPayload<O>::serialize( Serializer& serializer ) const
{
  serializer.integer( todo.content );
  serializer.integer<bool>( result.has_value() );
  if ( result.has_value() ) {
    serializer.integer( result.value().content );
  }
  serializer.integer<size_t>( handles.size() );
  for ( const auto& h : handles ) {
    serializer.integer( h.content );
  }
}

template<Message::Opcode O>
using TxP = TransferPayload<O>;

static constexpr Message::Opcode ACCEPT = Message::Opcode::ACCEPT_TRANSFER;
static constexpr Message::Opcode PROPOSE = Message::Opcode::PROPOSE_TRANSFER;

template TxP<PROPOSE> TxP<PROPOSE>::parse( Parser& parser );
template void TxP<PROPOSE>::serialize( Serializer& serializer ) const;
template TxP<ACCEPT> TxP<ACCEPT>::parse( Parser& parser );
template void TxP<ACCEPT>::serialize( Serializer& serializer ) const;
