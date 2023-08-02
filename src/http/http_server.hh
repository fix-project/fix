#pragma once

#include <queue>
#include <string>
#include <string_view>

#include "http_reader.hh"
#include "http_structures.hh"
#include "http_writer.hh"
#include "ring_buffer.hh"

class HTTPServer
{
  std::queue<HTTPResponse> pending_responses_ {};
  std::optional<HTTPRequestReader> request_reader_ {};
  std::optional<HTTPResponseWriter> response_writer_ {};

  HTTPRequestReader::State reader_state_ {};

public:
  void push_response( HTTPResponse&& res ) { pending_responses_.push( std::move( res ) ); }

  bool responses_empty() const { return pending_responses_.empty() and not response_writer_.has_value(); }

  void write( RingBuffer& out )
  {
    if ( not response_writer_.has_value() ) {
      if ( pending_responses_.empty() ) {
        throw std::runtime_error( "HTTPServer::write(): HTTPServer has no more responses" );
      }
      response_writer_.emplace( std::move( pending_responses_.front() ) );
      pending_responses_.pop();
    }

    response_writer_.value().write_to( out );

    if ( response_writer_.value().finished() ) {
      response_writer_.reset();
    }
  }

  bool read( RingBuffer& in, HTTPRequest& request )
  {
    if ( not request_reader_.has_value() ) {
      request_reader_.emplace( std::move( request ), std::move( reader_state_ ) );
    }

    in.pop( request_reader_.value().read( in.readable_region() ) );

    if ( request_reader_.value().finished() ) {
      request = request_reader_.value().release();
      reader_state_ = request_reader_.value().release_extra_state();
      request_reader_.reset();
      return true;
    }

    return false;
  }
};
