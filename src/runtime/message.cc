#include "message.hh"
#include "base64.hh"
#include "parser.hh"

using namespace std;

Message::Message( string_view header, string&& payload )
  : payload_( move( payload ) )
{
  if ( header.size() < HEADER_LENGTH ) {
    throw runtime_error( "Incomplete header." );
  }

  opcode_ = static_cast<Opcode>( header[8] );
}

Message::Message( const Opcode opcode, string&& payload )
  : opcode_( opcode )
  , payload_( payload )
{}

void Message::serialize_header( string& out )
{
  out.resize( HEADER_LENGTH );
  Serializer s( string_span::from_view( out ) );
  s.integer( payload_.length() );
  s.integer( static_cast<uint8_t>( opcode_ ) );

  if ( s.bytes_written() != HEADER_LENGTH ) {
    throw runtime_error( "Wrong header length" );
  }
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

void MessageParser::complete_message()
{
  completed_messages_.emplace( incomplete_header_, move( incomplete_payload_ ) );

  expected_payload_length_.reset();
  incomplete_header_.clear();
  incomplete_payload_.clear();
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
          complete_message();
        }
      }
    } else {
      const auto remaining_length
        = min( buf.length(), expected_payload_length_.value() - incomplete_payload_.length() );
      incomplete_payload_.append( buf.substr( 0, remaining_length ) );
      buf.remove_prefix( remaining_length );

      if ( incomplete_payload_.length() == expected_payload_length_.value() ) {
        complete_message();
      }
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
