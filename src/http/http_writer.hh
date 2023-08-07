#pragma once

#include "http_structures.hh"
#include "ring_buffer.hh"

template<class MessageType>
class HTTPWriter
{
  MessageType message_;

  size_t bytes_written_ { false };
  bool finished_ { false };

public:
  HTTPWriter( MessageType&& message );
  void write_to( RingBuffer& buffer );

  bool finished() const { return finished_; }
};

using HTTPRequestWriter = HTTPWriter<HTTPRequest>;
using HTTPResponseWriter = HTTPWriter<HTTPResponse>;
