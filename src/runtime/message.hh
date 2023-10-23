#pragma once

#include <algorithm>
#include <queue>
#include <string_view>
#include <variant>

#include "interface.hh"
#include "object.hh"
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
    BLOBDATA,
    TREEDATA,
    TAGDATA,
    PROPOSE_TRANSFER,
    ACCEPT_TRANSFER,
    COUNT,
  };

  static constexpr const char* OPCODE_NAMES[static_cast<uint8_t>( Opcode::COUNT )] = { "RUN",
                                                                                       "RESULT",
                                                                                       "REQUESTINFO",
                                                                                       "INFO",
                                                                                       "BLOBDATA",
                                                                                       "TREEDATA",
                                                                                       "TAGDATA",
                                                                                       "PROPOSE_TRANSFER",
                                                                                       "ACCEPT_TRANSFER" };

  constexpr static size_t HEADER_LENGTH = sizeof( size_t ) + sizeof( Opcode );

private:
  Opcode opcode_ { Opcode::COUNT };
  std::variant<std::string, std::string_view, Blob, Tree> payload_ {};

protected:
  Message( Opcode opcode, std::variant<std::string, std::string_view, Blob, Tree>&& payload )
    : opcode_( opcode )
    , payload_( std::move( payload ) )
  {}

public:
  Message() {};

  Opcode opcode() { return opcode_; }

  std::string_view payload();
  size_t payload_length();

  static Opcode opcode( std::string_view header );
  static size_t expected_payload_length( std::string_view header );

  Blob&& get_blob()
  {
    assert( holds_alternative<Blob>( payload_ ) );
    return std::move( get<Blob>( payload_ ) );
  }

  Tree&& get_tree()
  {
    assert( holds_alternative<Tree>( payload_ ) );
    return std::move( get<Tree>( payload_ ) );
  }

  void serialize_header( std::string& out );

  Message( Message&& ) = default;
  Message& operator=( Message&& ) = default;
  Message( const Message& ) = delete;
  Message& operator=( const Message& ) = delete;

  virtual ~Message() {}
};

struct RunPayload
{
  Task task {};

  static RunPayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static Message::Opcode OPCODE = Message::Opcode::RUN;
  size_t payload_length() const { return 43 + sizeof( Operation ); }
};

struct ResultPayload
{
  Task task {};
  Handle result {};

  static ResultPayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static Message::Opcode OPCODE = Message::Opcode::RESULT;
  size_t payload_length() const { return 43 + 43 + sizeof( Operation ); }
};

struct InfoPayload : public ITaskRunner::Info
{
  static InfoPayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static Message::Opcode OPCODE = Message::Opcode::INFO;
  size_t payload_length() const { return sizeof( uint32_t ); }
};

template<Message::Opcode O>
struct TransferPayload
{
  Task todo {};
  std::optional<Handle> result {};
  std::vector<Handle> handles {};

  static TransferPayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static Message::Opcode OPCODE = O;
  size_t payload_length() const
  {
    return 43 + sizeof( Operation ) + sizeof( bool ) + ( result ? 43 : 0 ) + +sizeof( size_t )
           + handles.size() * 43;
  }
};

using ProposeTransferPayload = TransferPayload<Message::Opcode::PROPOSE_TRANSFER>;
using AcceptTransferPayload = TransferPayload<Message::Opcode::ACCEPT_TRANSFER>;

using MessagePayload
  = std::variant<RunPayload, ResultPayload, InfoPayload, ProposeTransferPayload, AcceptTransferPayload>;

class IncomingMessage : public Message
{
public:
  IncomingMessage( std::string_view header, std::string&& payload );
  IncomingMessage( std::string_view header, Blob&& payload );
  IncomingMessage( std::string_view header, Tree&& payload );
};

class OutgoingMessage : public Message
{
public:
  OutgoingMessage() {};
  OutgoingMessage( const Opcode opcode, std::string&& payload );
  // Construct an non_owning data, backing data have to outlive the message
  OutgoingMessage( const Opcode opcode, std::string_view payload );

  static OutgoingMessage to_message( MessagePayload&& payload );
};

class MessageParser
{
private:
  std::optional<uint32_t> expected_payload_length_ {};

  std::string incomplete_header_ {};

  std::variant<std::string, Blob, Tree> incomplete_payload_ {};
  size_t completed_payload_length_ {};

  std::queue<IncomingMessage> completed_messages_ {};

  void complete_message();

public:
  size_t parse( std::string_view buf );

  bool empty() { return completed_messages_.empty(); }
  IncomingMessage& front() { return completed_messages_.front(); }
  void pop() { completed_messages_.pop(); }
  size_t size() { return completed_messages_.size(); }
};

template<class T>
std::string serialize( const T& obj )
{
  std::string out;
  out.resize( obj.payload_length() );
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
