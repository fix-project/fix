#pragma once

#include <queue>

#include "interface.hh"
#include "parser.hh"
#include "task.hh"

class Message
{
public:
  enum class Opcode : uint8_t
  {
    RUN,
    RESULT,
    REQUESTINFO,
    INFO,
    COUNT
  };

  static constexpr const char* OPCODE_NAMES[static_cast<uint8_t>( Opcode::COUNT )]
    = { "RUN", "RESULT", "REQUESTINFO", "INFO" };

  constexpr static size_t HEADER_LENGTH = sizeof( size_t ) + sizeof( Opcode );

private:
  Opcode opcode_ { Opcode::COUNT };
  std::string payload_ {};

public:
  Message() {};
  Message( std::string_view header, std::string&& payload );
  Message( const Opcode opcode, std::string&& payload );

  Opcode opcode() { return opcode_; }
  const std::string& payload() { return payload_; }
  static size_t expected_payload_length( std::string_view header );

  void serialize_header( std::string& out );

  Message( Message&& ) = default;
  Message& operator=( Message&& ) = default;
  Message( const Message& ) = delete;
  Message& operator=( const Message& ) = delete;
};

class MessageParser
{
private:
  std::optional<uint32_t> expected_payload_length_ {};

  std::string incomplete_header_ {};
  std::string incomplete_payload_ {};

  std::queue<Message> completed_messages_ {};

  void complete_message();

public:
  size_t parse( std::string_view buf );

  bool empty() { return completed_messages_.empty(); }
  Message& front() { return completed_messages_.front(); }
  void pop() { completed_messages_.pop(); }
  size_t size() { return completed_messages_.size(); }
};

struct RunPayload
{
  Task task {};

  static RunPayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static size_t PAYLOAD_LENGTH = 43 + sizeof( Operation );
};

struct ResultPayload
{
  Task task {};
  Handle result {};

  static ResultPayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static size_t PAYLOAD_LENGTH = 43 + 43 + sizeof( Operation );
};

struct InfoPayload : public ITaskRunner::Info
{
  static InfoPayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static size_t PAYLOAD_LENGTH = sizeof( uint32_t );
};

template<class T>
std::string serialize( const T& obj )
{
  std::string out;
  out.resize( T::PAYLOAD_LENGTH );
  Serializer s { string_span::from_view( out ) };
  obj.serialize( s );
  return out;
}

template<class T>
T parse( std::string_view in )
{
  Parser p { in };
  return T::parse( p );
}
