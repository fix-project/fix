#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

#include "base16.hh"
#include "eventloop.hh"
#include "handle.hh"
#include "http_server.hh"
#include "mmap.hh"
#include "operation.hh"
#include "relation.hh"
#include "runtime.hh"
#include "runtimestorage.hh"
#include "socket.hh"
#include "tester-utils.hh"

using namespace std;
using namespace boost::property_tree;

static constexpr size_t mebi = 1024 * 1024;

// Web server to expose computation graph through HTTP API for a viewer.
// Example client at fix/src/tester/http-client.

void kick_off( span_view<char*> args, vector<ReadOnlyFile>& open_files )
{
  // make the combination from the given arguments
  Handle encode_name = parse_args( args, open_files );

  if ( not args.empty() ) {
    throw runtime_error( "unexpected argument: "s + args.at( 0 ) );
  }

  // add the combination to the store, and print it
  cout << "Combination:\n" << pretty_print( encode_name ) << "\n";

  auto& runtime = Runtime::get_instance();

  // make a Thunk that points to the combination
  Handle thunk_name = encode_name.as_thunk();

  Handle result = runtime.eval( thunk_name );

  cout << "Thunk name: " << base16::encode( thunk_name ) << endl;
  cout << "Result name: " << base16::encode( result ) << endl << flush;
}

ptree serialize_relation( Relation relation )
{
  ptree pt;

  switch ( relation.type() ) {
    case RelationType::Apply: {
      pt.put( "relation", "Apply" );
      break;
    }
    case RelationType::COUNT: {
      break;
    }
    case RelationType::Eval: {
      pt.put( "relation", "Eval" );
      break;
    }
    case RelationType::Fill: {
      pt.put( "relation", "Fill" );
      break;
    }
  }

  pt.put( "lhs", base16::encode( relation.lhs() ) );
  pt.put( "rhs", base16::encode( relation.rhs() ) );

  return pt;
}

ptree get_explanations( Handle handle )
{
  ptree pt;
  auto [relations, handles] = Runtime::get_instance().get_explanations( handle );

  pt.push_back( ptree::value_type( "target", base16::encode( handle ) ) );

  ptree relation_tree;
  for ( auto& relation : relations ) {
    relation_tree.push_back( ptree::value_type( "", serialize_relation( relation ) ) );
  }
  pt.push_back( ptree::value_type( "relations", relation_tree ) );

  ptree pins_and_tags_tree;
  for ( auto& handle : handles ) {
    pins_and_tags_tree.push_back( ptree::value_type( "", base16::encode( handle ) ) );
  }
  pt.push_back( ptree::value_type( "handles", pins_and_tags_tree ) );

  return pt;
}

ptree get_task_relation( Task task )
{
  ptree pt;

  auto relation = Runtime::get_instance().get_task_relation( task );
  if ( relation.has_value() ) {
    pt.push_back( ptree::value_type( "relation", serialize_relation( relation.value() ) ) );
  }

  return pt;
}

ptree get_tree_contents( Handle handle )
{
  ptree pt;

  ptree handles;
  for ( const auto& entry : Runtime::get_instance().storage().get_tree( handle ) ) {
    handles.push_back( ptree::value_type( "", base16::encode( entry ) ) );
  }
  pt.push_back( ptree::value_type( "handles", handles ) );

  return pt;
}

// gets one definition if it exists. expects a=b&c=d or a=b
optional<pair<string, string>> find_definition( string& url,
                                                const char query_separator,
                                                const char definition_separator )
{
  if ( url.length() == 0 ) {
    return {};
  }
  size_t end_pos = url.find( query_separator );
  string definition = "";
  if ( end_pos == string::npos ) {
    end_pos = url.length();
    definition = url;
    url.erase();
  } else {
    definition = url.substr( 0, end_pos );
    url.erase( 0, end_pos + 1 );
  }

  size_t split_pos = definition.find( definition_separator );
  if ( split_pos == string::npos ) {
    throw runtime_error( "invalid url param, expected definition_separator" );
  }
  return { { definition.substr( 0, split_pos ), definition.substr( split_pos + 1 ) } };
}

// get the parameters from the url into a map.
// requires a url of the form blahblablah?a=b&d=91
unordered_map<string, string> decode_url_params( string url )
{
  size_t pos = url.find( "?" );
  if ( pos == string::npos ) {
    return {};
  }
  url.erase( 0, pos + 1 );
  unordered_map<string, string> result = {};
  auto definition = find_definition( url, '&', '=' );
  while ( definition.has_value() ) {
    result.insert( definition.value() );
    definition = find_definition( url, '&', '=' );
  }

  return result;
}

// Try to read a file to a string. Returns a tuple of the file content and
// the inferred mime type based on the file extension.
optional<tuple<string, string>> try_read_file( string directory, string file_name )
{
  // Disallow going up directories.
  if ( file_name.find( ".." ) != string::npos ) {
    return {};
  }
  std::ifstream ifs( directory + "/" + file_name );
  // File must exist.
  if ( !ifs.good() ) {
    return {};
  }
  std::string content( ( std::istreambuf_iterator<char>( ifs ) ), ( std::istreambuf_iterator<char>() ) );
  std::string content_type = "";
  if ( file_name.ends_with( ".html" ) ) {
    content_type = "text/html";
  } else if ( file_name.ends_with( ".wasm" ) ) {
    content_type = "application/wasm";
  } else if ( file_name.ends_with( ".js" ) ) {
    content_type = "application/javascript";
  } else {
    return {};
  }
  return make_tuple( content, content_type );
}

optional<tuple<string, string>> try_get_response( string target, string source_directory )
{
  if ( not target.starts_with( '/' ) ) {
    return {};
  }
  target = target.substr( 1 );
  // Serve static files.
  if ( target == "" or target == "index.html" ) {
    return try_read_file( source_directory, "index.html" );
  } else if ( target.starts_with( "fix_viewer" ) and target.ends_with( "_bg.wasm" ) ) {
    return try_read_file( source_directory, target );
  } else if ( target.starts_with( "fix_viewer" ) and target.ends_with( ".js" ) ) {
    return try_read_file( source_directory, target );
  }

  // Handle API calls.
  auto map = decode_url_params( target );
  if ( target.starts_with( "explanations" ) ) {
    stringstream ptree;
    write_json( ptree, get_explanations( base16::decode( map.at( "handle" ) ) ) );
    return make_tuple( ptree.str(), "text/json" );
  } else if ( target.starts_with( "relation" ) ) {
    stringstream ptree;
    write_json(
      ptree,
      get_task_relation( Task( base16::decode( map.at( "handle" ) ), Operation( stoul( map.at( "op" ) ) ) ) ) );
    return make_tuple( ptree.str(), "text/json" );
  } else if ( target.starts_with( "tree_contents" ) ) {
    stringstream ptree;
    write_json( ptree, get_tree_contents( ( base16::decode( map.at( "handle" ) ) ) ) );
    return make_tuple( ptree.str(), "text/json" );
  }

  return {};
}

struct EventLoopCategories
{
  size_t read, respond, write;

  EventLoopCategories( EventLoop& loop )
    : read( loop.add_category( "socket read" ) )
    , respond( loop.add_category( "HTTP response" ) )
    , write( loop.add_category( "socket write" ) )
  {}
};

// Encapsulates the logic for handling a single http connection.
class Connection
{
  TCPSocket sock_;
  size_t id_;
  Address peer_;
  bool dead_ {};

  RingBuffer incoming_ { mebi };
  RingBuffer outgoing_ { mebi };

  HTTPServer http_server_ {};
  HTTPRequest req_in_progress_ {};

  vector<EventLoop::RuleHandle> rules_ {};
  string source_directory_;

  void handle_request( const HTTPRequest& request )
  {
    cerr << "Incoming request: " << request.request_target << "\n";
    std::string target = request.request_target;
    optional<tuple<string, string>> response = try_get_response( target, source_directory_ );

    HTTPResponse the_response;
    the_response.http_version = "HTTP/1.1";
    if ( response.has_value() ) {
      const auto& [response_string, content_type] = response.value();
      the_response.status_code = "200";
      the_response.reason_phrase = "OK";
      the_response.headers.content_type = content_type;
      the_response.headers.content_length = response_string.size();
      the_response.headers.access_control_allow_origin = "http://127.0.0.1:8800";
      the_response.body = response_string;
    } else {
      the_response.status_code = "404";
      the_response.reason_phrase = "Not Found";
      string not_found = "API not found, connection id = " + to_string( id_ );
      the_response.headers.content_type = "text/plain";
      the_response.headers.content_length = not_found.size();
      the_response.body = not_found;
    }

    http_server_.push_response( move( the_response ) );
  }

public:
  Connection( size_t id,
              TCPSocket&& sock,
              EventLoop& loop,
              const EventLoopCategories& categories,
              string source_directory )
    : sock_( std::move( sock ) )
    , id_( id )
    , peer_( sock_.peer_address() )
    , source_directory_( source_directory )
  {
    sock_.set_blocking( false );

    rules_.push_back( loop.add_rule(
      categories.read,
      sock_,
      Direction::In,
      [&] {
        incoming_.push_from_fd( sock_ );
        if ( http_server_.read( incoming_, req_in_progress_ ) ) {
          handle_request( req_in_progress_ );
        }
      },
      [&] { return incoming_.can_write(); },
      [&] { dead_ = true; } ) );

    rules_.push_back( loop.add_rule(
      categories.respond,
      [&] { http_server_.write( outgoing_ ); },
      [&] { return outgoing_.can_write() and not http_server_.responses_empty(); } ) );

    rules_.push_back( loop.add_rule(
      categories.write,
      sock_,
      Direction::Out,
      [&] { outgoing_.pop_to_fd( sock_ ); },
      [&] { return outgoing_.can_read(); },
      [&] { dead_ = true; } ) );
  }

  bool dead() const { return dead_; }

  ~Connection()
  {
    for ( auto& rule : rules_ ) {
      rule.cancel();
    }
    rules_.clear(); // XXX???XXX beware of possible issues
                    // if rules_ is already destroyed
  }
};

int min_args = 4;
int max_args = -1;

void program_body( span_view<char*> args )
{
  ios::sync_with_stdio( false );

  // Start listening on the specified port.
  TCPSocket server_socket {};
  server_socket.set_reuseaddr();
  server_socket.bind( { "127.0.0.1", args.at( 1 ) } );
  server_socket.set_blocking( false );
  server_socket.listen();
  cerr << "Listening on port " << server_socket.local_address().port() << "\n";

  // Process the arguments: source_directory and fix program
  string source_directory = args.at( 2 );
  vector<ReadOnlyFile> open_files;
  if ( source_directory.ends_with( '/' ) ) {
    source_directory.pop_back();
  }
  args.remove_prefix( 3 );
  kick_off( args, open_files );

  EventLoop events;
  EventLoopCategories event_categories { events };

  list<Connection> connections;
  size_t connection_id_counter {};

  events.add_rule( "new TCP connection", server_socket, Direction::In, [&] {
    connections.emplace_back(
      connection_id_counter++, server_socket.accept(), events, event_categories, string( source_directory ) );
  } );

  do {
    connections.remove_if( []( auto& x ) { return x.dead(); } );
  } while ( events.wait_next_event( 500 ) != EventLoop::Result::Exit );

  cerr << "Exiting...\n";
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " PORT SOURCE_DIR entry...\n";
  cerr << "   entry :=   file:<filename>\n";
  cerr << "            | string:<string>\n";
  cerr << "            | name:<base16-encoded name>\n";
  cerr << "            | uint<n>:<integer> (with <n> = 8 | 16 | 32 | 64)\n";
  cerr << "            | tree:<n> (followed by <n> entries)\n";
  cerr << "            | thunk: (followed by tree:<n>)\n";
  cerr << "            | compile:<filename>\n";
  cerr << "            | ref:<ref>\n";
}
