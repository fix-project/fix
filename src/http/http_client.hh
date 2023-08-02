#pragma once

#include <queue>
#include <string>
#include <string_view>

#include "http_reader.hh"
#include "http_structures.hh"
#include "http_writer.hh"
#include "ring_buffer.hh"

class HTTPClient
{
  std::queue<HTTPRequest> pending_requests_ {};
  std::queue<bool> requests_are_head_ {};
  std::optional<HTTPRequestWriter> request_writer_ {};
  std::optional<HTTPResponseReader> response_reader_ {};

  HTTPResponseReader::State reader_state_ {};

public:
  void push_request( HTTPRequest&& req )
  {
    constexpr std::string_view HEAD = "HEAD";
    requests_are_head_.push( req.method == HEAD );
    pending_requests_.push( std::move( req ) );
  }

  bool requests_empty() const { return pending_requests_.empty() and not request_writer_.has_value(); }

  void write( RingBuffer& out )
  {
    if ( not request_writer_.has_value() ) {
      if ( pending_requests_.empty() ) {
        throw std::runtime_error( "HTTPClient::write(): HTTPClient has no more requests" );
      }
      request_writer_.emplace( std::move( pending_requests_.front() ) );
      pending_requests_.pop();
    }

    request_writer_.value().write_to( out );

    if ( request_writer_.value().finished() ) {
      request_writer_.reset();
    }
  }

  bool read( RingBuffer& in, HTTPResponse& response )
  {
    if ( not response_reader_.has_value() ) {
      if ( requests_are_head_.empty() ) {
        throw std::runtime_error( "HTTPClient::read(): HTTPClient has no more requests" );
      }

      response_reader_.emplace( requests_are_head_.front(), std::move( response ), std::move( reader_state_ ) );
    }

    in.pop( response_reader_.value().read( in.readable_region() ) );

    if ( response_reader_.value().finished() ) {
      response = response_reader_.value().release();
      reader_state_ = response_reader_.value().release_extra_state();
      response_reader_.reset();
      return true;
    }

    return false;
  }
};
