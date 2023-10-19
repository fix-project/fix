#include "message.hh"
#include "base64.hh"
#include "object.hh"
#include "parser.hh"

using namespace std;

IncomingMessage::IncomingMessage( string_view header, string&& payload )
  : Message( Message::opcode( header ), move( payload ) )
{}

IncomingMessage::IncomingMessage( string_view header, Blob&& payload )
  : Message( Message::opcode( header ), move( payload ) )
{}

IncomingMessage::IncomingMessage( string_view header, Tree&& payload )
  : Message( Message::opcode( header ), move( payload ) )
{}

OutgoingMessage::OutgoingMessage( const Opcode opcode, string&& payload )
  : Message( opcode, move( payload ) )
{}

OutgoingMessage::OutgoingMessage( const Opcode opcode, string_view payload )
  : Message( opcode, payload )
{}

void Message::serialize_header( string& out )
{
  out.resize( HEADER_LENGTH );
  Serializer s( string_span::from_view( out ) );
  s.integer( payload_length() );
  s.integer( static_cast<uint8_t>( opcode_ ) );

  if ( s.bytes_written() != HEADER_LENGTH ) {
    throw runtime_error( "Wrong header length" );
  }
}

string_view Message::payload()
{
  return std::visit(
    [&]( auto& arg ) -> string_view {
      return { reinterpret_cast<const char*>( arg.data() ), payload_length() };
    },
    payload_ );
}

size_t Message::payload_length()
{
  if ( std::holds_alternative<Tree>( payload_ ) ) {
    return span_view<Handle>( get<Tree>( payload_ ) ).byte_size();
  } else {
    return std::visit( []( auto& arg ) -> size_t { return arg.size(); }, payload_ );
  }
}

Message::Opcode Message::opcode( string_view header )
{
  assert( header.size() == HEADER_LENGTH );
  return static_cast<Opcode>( header[8] );
}

size_t Message::expected_payload_length( string_view header )
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
  return std::visit(
    []( auto&& p ) -> OutgoingMessage {
      using T = std::decay_t<decltype( p )>;
      return { T::OPCODE, serialize( p ) };
    },
    payload );
}

void MessageParser::complete_message()
{
  std::visit( [&]( auto&& arg ) { completed_messages_.emplace( incomplete_header_, move( arg ) ); },
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
        expected_payload_length_ = Message::expected_payload_length( incomplete_header_ );

        if ( expected_payload_length_.value() == 0 ) {
          assert( Message::opcode( incomplete_header_ ) == Message::Opcode::REQUESTINFO );
          complete_message();
        } else {
          switch ( Message::opcode( incomplete_header_ ) ) {
            case Message::Opcode::RUN:
            case Message::Opcode::INFO:
            case Message::Opcode::RESULT: {
              incomplete_payload_ = "";
              get<string>( incomplete_payload_ ).resize( expected_payload_length_.value() );
              break;
            }

            case Message::Opcode::BLOBDATA: {
              incomplete_payload_ = Blob( expected_payload_length_.value() );
              break;
            }

            case Message::Opcode::TREEDATA:
            case Message::Opcode::TAGDATA: {
              incomplete_payload_ = Tree( expected_payload_length_.value() / sizeof( Handle ) );
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

Handle parse_handle( Parser& parser )
{
  string handle;
  handle.resize( 43 );
  parser.string( string_span::from_view( handle ) );

  if ( parser.error() ) {
    throw runtime_error( "Failed to parse handle." );
  }

  return base64::decode( handle );
}

Operation parse_operation( Parser& parser )
{
  uint8_t operation;
  parser.integer( operation );

  if ( parser.error() ) {
    throw runtime_error( "Failed to parse opeartion." );
  }

  return static_cast<Operation>( operation );
}

RunPayload RunPayload::parse( Parser& parser )
{
  return { .task { parse_handle( parser ), parse_operation( parser ) } };
}

void RunPayload::serialize( Serializer& serializer ) const
{
  serializer.string( base64::encode( task.handle() ) );
  serializer.integer( static_cast<uint8_t>( task.operation() ) );
}

ResultPayload ResultPayload::parse( Parser& parser )
{
  return { .task { parse_handle( parser ), parse_operation( parser ) }, .result { parse_handle( parser ) } };
}

void ResultPayload::serialize( Serializer& serializer ) const
{
  serializer.string( base64::encode( task.handle() ) );
  serializer.integer( static_cast<uint8_t>( task.operation() ) );
  serializer.string( base64::encode( result ) );
}

InfoPayload InfoPayload::parse( Parser& parser )
{
  uint32_t parallelism;
  parser.integer( parallelism );

  if ( parser.error() ) {
    throw runtime_error( "Failed to parse opeartion." );
  }

  return { { .parallelism = parallelism } };
}

void InfoPayload::serialize( Serializer& serializer ) const
{
  serializer.integer( parallelism );
}
