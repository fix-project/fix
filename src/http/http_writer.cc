#include "http_writer.hh"

using namespace std;

class WriteAttempt
{
  RingBuffer& buffer_;
  size_t& bytes_written_;
  size_t remaining_offset_;
  bool completed_ { true };

public:
  WriteAttempt( RingBuffer& buffer, size_t& bytes_written )
    : buffer_( buffer )
    , bytes_written_( bytes_written )
    , remaining_offset_( bytes_written )
  {}

  void write( std::string_view str )
  {
    if ( remaining_offset_ >= str.size() ) {
      remaining_offset_ -= str.size();
      return;
    }

    if ( remaining_offset_ > 0 ) {
      str.remove_prefix( remaining_offset_ );
      remaining_offset_ = 0;
    }

    const size_t bytes_written_this_write = buffer_.push_from_const_str( str );
    if ( bytes_written_this_write != str.size() ) {
      completed_ = false;
    }
    bytes_written_ += bytes_written_this_write;
  }

  bool completed() const { return completed_; }

  void write_start_line( const HTTPRequest& request )
  {
    write( request.method );
    write( " " );
    write( request.request_target );
    write( " " );
    write( request.http_version );
    write( "\r\n" );
  }

  void write_start_line( const HTTPResponse& response )
  {
    write( response.http_version );
    write( " " );
    write( response.status_code );
    write( " " );
    write( response.reason_phrase );
    write( "\r\n" );
  }
};

template<class MessageType>
HTTPWriter<MessageType>::HTTPWriter( MessageType&& message )
  : message_( move( message ) )
{}

void maybe_print( const string_view name, const string_view value, WriteAttempt& attempt )
{
  if ( not value.empty() ) {
    attempt.write( name );
    attempt.write( ": " );
    attempt.write( value );
    attempt.write( "\r\n" );
  }
}

template<class MessageType>
void HTTPWriter<MessageType>::write_to( RingBuffer& buffer )
{
  WriteAttempt attempt { buffer, bytes_written_ };

  /* write start line (request or status ) */
  attempt.write_start_line( message_ );

  /* write headers */
  if ( message_.headers.content_length.has_value() ) {
    attempt.write( "Content-Length: " );
    attempt.write( to_string( message_.headers.content_length.value() ) );
    attempt.write( "\r\n" );
  }

  maybe_print( "Host", message_.headers.host, attempt );
  maybe_print( "Content-type", message_.headers.content_type, attempt );
  maybe_print( "Connection", message_.headers.connection, attempt );
  maybe_print( "Upgrade", message_.headers.upgrade, attempt );
  maybe_print( "Origin", message_.headers.origin, attempt );
  maybe_print( "Sec-WebSocket-Key", message_.headers.sec_websocket_key, attempt );
  maybe_print( "Sec-WebSocket-Accept", message_.headers.sec_websocket_accept, attempt );
  maybe_print( "Location", message_.headers.location, attempt );

  /* end of headers */
  attempt.write( "\r\n" );

  /* write body */
  attempt.write( message_.body );

  if ( attempt.completed() ) {
    finished_ = true;
  }
}

template class HTTPWriter<HTTPRequest>;
template class HTTPWriter<HTTPResponse>;
