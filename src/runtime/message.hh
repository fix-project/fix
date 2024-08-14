#pragma once

#include <queue>
#include <span>
#include <variant>
#include <vector>

#include "handle.hh"
#include "interface.hh"
#include "object.hh"
#include "parser.hh"
#include "types.hh"

class Message
{
public:
  enum class Opcode : uint8_t
  {
    RUN,
    RESULT,
    REQUESTINFO,
    INFO,
    REQUESTTREE,
    REQUESTSHALLOWTREE,
    REQUESTBLOB,
    BLOBDATA,
    TREEDATA,
    SHALLOWTREEDATA,
    PROPOSE_TRANSFER,
    ACCEPT_TRANSFER,
    COUNT,
  };

  static constexpr const char* OPCODE_NAMES[static_cast<uint8_t>( Opcode::COUNT )] = { "RUN",
                                                                                       "RESULT",
                                                                                       "REQUESTINFO",
                                                                                       "INFO",
                                                                                       "REQUESTTREE",
                                                                                       "REQUESTSHALLOWTREE",
                                                                                       "REQUESTBLOB",
                                                                                       "BLOBDATA",
                                                                                       "TREEDATA",
                                                                                       "SHALLOWTREEDATA",
                                                                                       "PROPOSE_TRANSFER",
                                                                                       "ACCEPT_TRANSFER" };

  constexpr static size_t HEADER_LENGTH = sizeof( size_t ) + sizeof( Opcode );

private:
  Opcode opcode_ { Opcode::COUNT };

protected:
  Message( Opcode opcode )
    : opcode_( opcode )
  {}

public:
  Opcode opcode() { return opcode_; }

  static Opcode opcode( std::string_view header );
};

struct RunPayload
{
  Handle<Relation> task {};

  static RunPayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static Message::Opcode OPCODE = Message::Opcode::RUN;
  size_t payload_length() const { return sizeof( u8x32 ); }
};

struct ResultPayload
{
  Handle<Relation> task {};
  Handle<Object> result {};

  static ResultPayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static Message::Opcode OPCODE = Message::Opcode::RESULT;
  size_t payload_length() const { return 2 * sizeof( u8x32 ); }
};

struct RequestBlobPayload
{
  Handle<Named> handle;

  static RequestBlobPayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static Message::Opcode OPCODE = Message::Opcode::REQUESTBLOB;
  size_t payload_length() const { return sizeof( u8x32 ); }
};

struct RequestTreePayload
{
  Handle<AnyTree> handle {};

  static RequestTreePayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static Message::Opcode OPCODE = Message::Opcode::REQUESTTREE;
  size_t payload_length() const { return sizeof( u8x32 ); }
};

struct RequestShallowTreePayload
{
  Handle<AnyTree> handle {};

  static RequestShallowTreePayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static Message::Opcode OPCODE = Message::Opcode::REQUESTSHALLOWTREE;
  size_t payload_length() const { return sizeof( u8x32 ); }
};

struct InfoPayload
{
  uint32_t parallelism {};
  double link_speed {};
  std::unordered_set<Handle<AnyDataType>> data {};

  static InfoPayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static Message::Opcode OPCODE = Message::Opcode::INFO;
  size_t payload_length() const
  {
    return sizeof( uint32_t ) + sizeof( double ) + sizeof( size_t ) + data.size() * sizeof( u8x32 );
  }
};

template<Message::Opcode O>
struct TransferPayload
{
  Handle<Relation> todo {};
  std::optional<Handle<Object>> result {};
  std::vector<Handle<AnyDataType>> handles {};

  static TransferPayload parse( Parser& parser );
  void serialize( Serializer& serializer ) const;

  constexpr static Message::Opcode OPCODE = O;
  size_t payload_length() const
  {
    return sizeof( u8x32 ) + sizeof( bool ) + ( result ? sizeof( u8x32 ) : 0 ) + sizeof( size_t )
           + handles.size() * sizeof( u8x32 );
  }
};

using ProposeTransferPayload = TransferPayload<Message::Opcode::PROPOSE_TRANSFER>;
using AcceptTransferPayload = TransferPayload<Message::Opcode::ACCEPT_TRANSFER>;

using BlobDataPayload = std::pair<Handle<Named>, BlobData>;
using TreeDataPayload = std::pair<Handle<AnyTree>, TreeData>;

struct ShallowTreeDataPayload
{
  Handle<AnyTree> handle;
  TreeData data;

  ShallowTreeDataPayload( Handle<AnyTree> handle, TreeData data )
    : handle( handle )
    , data( data )
  {}
};

using MessagePayload = std::variant<RunPayload,
                                    ResultPayload,
                                    InfoPayload,
                                    ProposeTransferPayload,
                                    AcceptTransferPayload,
                                    RequestBlobPayload,
                                    RequestTreePayload,
                                    RequestShallowTreePayload,
                                    BlobDataPayload,
                                    TreeDataPayload,
                                    ShallowTreeDataPayload>;

class IncomingMessage : public Message
{
  std::variant<std::string, OwnedMutBlob, OwnedMutTree> payload_;

public:
  IncomingMessage( const Message::Opcode opcode, std::string&& payload );
  IncomingMessage( const Message::Opcode opcode, OwnedMutBlob&& payload );
  IncomingMessage( const Message::Opcode opcode, OwnedMutTree&& payload );

  BlobData get_blob()
  {
    assert( holds_alternative<OwnedMutBlob>( payload_ ) );
    return make_shared<OwnedBlob>( std::move( get<OwnedMutBlob>( payload_ ) ) );
  }

  TreeData get_tree()
  {
    assert( holds_alternative<OwnedMutTree>( payload_ ) );
    return make_shared<OwnedTree>( std::move( get<OwnedMutTree>( payload_ ) ) );
  }

  static size_t expected_payload_length( std::string_view header );
  auto& payload() { return payload_; }
};

class OutgoingMessage : public Message
{
  std::variant<BlobData, TreeData, std::string> payload_ {};

public:
  OutgoingMessage( const Message::Opcode opcode, BlobData payload );
  OutgoingMessage( const Message::Opcode opcode, TreeData payload );
  OutgoingMessage( const Message::Opcode opcode, std::string&& payload );

  static OutgoingMessage to_message( MessagePayload&& payload );
  std::string_view payload();
  void serialize_header( std::string& out );
  size_t payload_length();
};

class MessageParser
{
private:
  std::optional<uint32_t> expected_payload_length_ {};

  std::string incomplete_header_ {};

  std::variant<std::string, OwnedMutBlob, OwnedMutTree> incomplete_payload_ {};
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
  Serializer s { out };
  obj.serialize( s );
  return out;
}

template<class T>
T parse( std::string_view in )
{
  Parser p { in };
  return T::parse( p );
}
