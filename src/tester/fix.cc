#include <cstdlib>
#include <format>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>

#include <glog/logging.h>

#include "option-parser.hh"

#include "base16.hh"
#include "object.hh"
#include "overload.hh"
#include "readonlyrt.hh"
#include "repository.hh"
#include "storage_exception.hh"
#include "tester-utils.hh"

using namespace std;

extern map<string, pair<function<void( int, char*[] )>, const char*>> commands;

namespace blob {
void add( int argc, char* argv[] )
{
  OptionParser parser( "add-blob", commands["add-blob"].second );
  const char* filename = NULL;
  std::optional<const char*> label;
  parser.AddArgument(
    "filename", OptionParser::ArgumentCount::One, [&]( const char* argument ) { filename = argument; } );
  parser.AddOption(
    'l', "label", "label", "Assign a human-readable name to this Blob.", [&]( const char* argument ) {
      label = argument;
    } );
  parser.Parse( argc, argv );
  if ( !filename )
    exit( EXIT_FAILURE );

  if ( not std::filesystem::exists( filename ) ) {
    cerr << "Error: file \"" << filename << "\" does not exist.\n";
    exit( EXIT_FAILURE );
  }

  Repository storage;

  try {
    Handle<Fix> handle = storage.create( std::make_shared<OwnedBlob>( filename ) );
    if ( label )
      storage.label( *label, handle );
    cout << handle.content << endl;
  } catch ( std::filesystem::filesystem_error& e ) {
    cerr << "Error: unable to open file " << std::filesystem::path( filename ) << ":\n\t" << e.what() << "\n";
    exit( EXIT_FAILURE );
  }
}

void create( int argc, char* argv[] )
{
  OptionParser parser( "create-blob", commands["create-blob"].second );
  static std::optional<std::string> contents;
  std::optional<const char*> label;
  parser.AddArgument( "contents", OptionParser::ArgumentCount::ZeroOrMore, [&]( const char* argument ) {
    if ( contents ) {
      *contents += std::string( " " ) + argument;
    } else {
      contents = argument;
    }
  } );
  parser.AddOption(
    'l', "label", "label", "Assign a human-readable name to this Blob.", [&]( const char* argument ) {
      label = argument;
    } );
  parser.Parse( argc, argv );

  Repository storage;
  if ( !contents ) {
    std::istreambuf_iterator<char> begin( std::cin ), end;
    contents = { begin, end };
  }

  Handle<Fix> handle = storage.create(
    std::make_shared<OwnedBlob>( std::span { contents->data(), contents->size() }, AllocationType::Static ) );
  if ( label )
    storage.label( *label, handle );
  cout << handle.content << endl;
}

void cat( int argc, char* argv[] )
{
  OptionParser parser( "cat-blob", commands["cat-blob"].second );
  const char* ref = NULL;
  parser.AddArgument(
    "label-or-hash", OptionParser::ArgumentCount::One, [&]( const char* argument ) { ref = argument; } );
  parser.Parse( argc, argv );
  if ( !ref )
    exit( EXIT_FAILURE );

  Repository storage;
  auto handle = storage.lookup( ref );
  auto blob = handle::extract<Blob>( handle );
  if ( !blob ) {
    cerr << "Error: \"" << ref << "\" does not describe a Blob.\n";
    exit( EXIT_FAILURE );
  }
  blob->visit<void>( overload {
    []( Handle<Literal> literal ) { cout << literal.view(); },
    [&]( Handle<Named> named ) {
      auto data = storage.get( named ).value();
      cout << std::string( data->span().begin(), data->span().end() );
    },
  } );
}
}

namespace tree {
void create( int argc, char* argv[] )
{
  OptionParser parser( "create-tree", commands["create-tree"].second );
  static std::vector<Handle<Fix>> contents;
  std::optional<const char*> label;
  Repository storage;
  parser.AddOption(
    'l', "label", "label", "Assign a human-readable name to this Tree.", [&]( const char* argument ) {
      label = argument;
    } );
  parser.AddArgument( "handles", OptionParser::ArgumentCount::ZeroOrMore, [&]( const char* argument ) {
    std::string ref( argument );
    auto handle = storage.lookup( ref );
    contents.push_back( handle );
  } );
  parser.Parse( argc, argv );

  if ( contents.size() == 0 ) {
    while ( !cin.eof() ) {
      std::string ref;
      std::getline( cin, ref );
      auto handle = storage.lookup( ref );
      contents.push_back( handle );
    }
  }

  Handle<Fix> handle = storage
                         .create( std::make_shared<OwnedTree>( std::span { contents.data(), contents.size() },
                                                               AllocationType::Static ) )
                         .visit<Handle<Fix>>( []( const auto x ) { return x; } );
  if ( label )
    storage.label( *label, handle );
  cout << handle.content << endl;
}

void cat( int argc, char* argv[] )
{
  OptionParser parser( "cat-tree", commands["cat-tree"].second );
  const char* ref = NULL;
  parser.AddArgument(
    "label-or-hash", OptionParser::ArgumentCount::One, [&]( const char* argument ) { ref = argument; } );
  parser.Parse( argc, argv );
  if ( !ref )
    exit( EXIT_FAILURE );

  Repository storage;
  auto handle = storage.lookup( ref );
  handle::data( handle ).visit<void>( overload {
    [&]( Handle<AnyTree> tree ) {
      auto data = storage.get( tree ).value();
      for ( size_t i = 0; i < data->size(); i++ ) {
        cout << data->at( i ).content << "\n";
      }
    },
    [&]( auto ) {
      cerr << "Error: \"" << ref << "\" does not describe a Tree.\n";
      exit( EXIT_FAILURE );
    },
  } );
}

void print_tree( Repository& storage, Handle<AnyTree> handle, bool decode )
{
  auto tree = storage.get( handle ).value();
  for ( size_t i = 0; i < tree->size(); i++ ) {
    if ( decode )
      cout << "  " << i << ". " << tree->at( i ) << "\n";
    else
      cout << "  " << i << ". " << tree->at( i ).content << "\n";
  }
}

void ls( int argc, char* argv[] )
{
  OptionParser parser( "ls-tree", commands["ls-tree"].second );
  const char* ref = NULL;
  bool decode = false;
  parser.AddArgument(
    "label-or-hash", OptionParser::ArgumentCount::One, [&]( const char* argument ) { ref = argument; } );
  parser.AddOption( 'd', "decode", "Decode Handles within Tree.", [&] { decode = true; } );
  parser.Parse( argc, argv );
  if ( !ref )
    exit( EXIT_FAILURE );

  Repository storage;
  auto handle = handle::data( storage.lookup( ref ) )
                  .visit<Handle<AnyTree>>( overload {
                    []( Handle<AnyTree> x ) { return x; },
                    [&]( auto ) -> Handle<AnyTree> {
                      cerr << std::format( "Ref {} does not a describe a tree.", ref );
                      exit( EXIT_FAILURE );
                    },
                  } );
  print_tree( storage, handle, decode );
}
}

void labels( int argc, char* argv[] )
{
  OptionParser parser( "labels", commands["labels"].second );
  bool handles = false;
  bool decode = false;
  parser.AddOption( 'h', "handles", "Also output the referenced Handles.", [&] { handles = true; } );
  parser.AddOption( 'd', "decode", "Decode the referenced Handles.", [&] {
    handles = true;
    decode = true;
  } );
  parser.Parse( argc, argv );
  Repository storage;

  auto unordered_labels = storage.labels();
  set<std::string> labels( unordered_labels.begin(), unordered_labels.end() );

  size_t length = 0;
  for ( const auto& label : labels ) {
    if ( label.size() > length ) {
      length = label.size();
    }
  }

  for ( const auto& label : labels ) {
    if ( handles ) {
      cout << std::format( "{:{}} -> ", label, length );
      if ( not decode ) {
        cout << storage.labeled( label ) << "\n";
      } else {
        cout << storage.labeled( label ) << "\n";
      }
    } else {
      cout << label << "\n";
    }
  }
}

void gc( int argc, char* argv[] )
{
  OptionParser parser( "gc", commands["gc"].second );
  bool dry_run = false;
  parser.AddOption( 'n', "dry-run", "Output the list of data to be deleted without modifying the repository.", [&] {
    dry_run = true;
  } );
  parser.Parse( argc, argv );
  Repository storage;

  auto labels = storage.labels();
  unordered_set<Handle<Fix>> roots;
  for ( const auto& label : labels ) {
    roots.insert( storage.labeled( label ) );
  }

  unordered_set<Handle<Fix>> data;
  for ( const auto& datum : std::filesystem::directory_iterator( storage.path() / "data" ) ) {
    data.insert( Handle<Fix>::forge( base16::decode( datum.path().filename().string() ) ) );
  }

  unordered_set<Handle<Fix>> needed;
  for ( const auto root : roots ) {
    if ( root.contains<Relation>() ) {
      needed.insert(
        handle::data( storage.get( root.unwrap<Relation>() ).value() ).visit<Handle<Fix>>( []( const auto x ) {
          return x;
        } ) );
    }
    needed.insert( handle::data( root ).visit<Handle<Fix>>( []( const auto x ) { return x; } ) );
    auto pins = storage.pinned( root );
    for ( const auto& p : pins ) {
      needed.insert( handle::data( p ).visit<Handle<Fix>>( []( const auto x ) { return x; } ) );
    }
  }

  unordered_set<Handle<Fix>> unneeded;
  for ( const auto datum : data ) {
    if ( not needed.contains( datum ) )
      unneeded.insert( datum );
  }

  size_t total_size = 0;
  for ( const auto x : unneeded ) {
    std::visit( overload {
                  []( const Handle<Relation> ) {},
                  [&]( const auto x ) { total_size += handle::size( x ) * sizeof( Handle<Fix> ); },
                },
                handle::data( x ).get() );
  }

  if ( dry_run ) {
    for ( const auto x : unneeded ) {
      cout << x.content << "\n";
    }
    cout << "Would have deleted " << total_size << " bytes.\n";
  } else {
    for ( const auto x : unneeded ) {
      auto name = x.content;
      std::filesystem::remove( storage.path() / "data" / base16::encode( name ) );
    }
    cout << "Deleted " << total_size << " bytes.\n";
  }
}

void label( int argc, char* argv[] )
{
  OptionParser parser( "label", commands["label"].second );
  const char* ref = NULL;
  const char* new_label = NULL;
  parser.AddArgument(
    "label-or-hash", OptionParser::ArgumentCount::One, [&]( const char* argument ) { ref = argument; } );
  parser.AddArgument(
    "new-label", OptionParser::ArgumentCount::One, [&]( const char* argument ) { new_label = argument; } );
  parser.Parse( argc, argv );
  if ( !ref or !new_label )
    exit( EXIT_FAILURE );
  Repository storage;

  Handle<Fix> handle = storage.lookup( ref );
  storage.label( new_label, handle );
}

void eval( int argc, char* argv[] )
{
  if ( argc == 0 or string( argv[1] ) == "--help" ) {
    parser_usage_message();
    exit( EXIT_FAILURE );
  }

  auto rt = ReadOnlyTester::init();
  span_view<char*> args = { argv, static_cast<size_t>( argc ) };
  args.remove_prefix( 1 );
  auto handle = parse_args( *rt, args );

  if ( !handle::extract<Object>( handle ).has_value() ) {
    cerr << "Handle is not an Object";
    exit( EXIT_FAILURE );
  }

  auto res = rt->executor().execute( Handle<Eval>( handle::extract<Object>( handle ).value() ) );
  cout << res << endl;
}

void init( int, char*[] )
{
  bool exists = false;
  if ( std::filesystem::exists( ".fix" ) ) {
    if ( std::filesystem::is_directory( ".fix" ) ) {
      exists = true;
    } else {
      cerr << "Error: \".fix\" already exists, but is not a directory.\n";
      exit( EXIT_FAILURE );
    }
  }
  std::filesystem::create_directory( ".fix" );
  std::filesystem::create_directory( ".fix/data" );
  std::filesystem::create_directory( ".fix/relations" );
  std::filesystem::create_directory( ".fix/labels" );
  std::filesystem::create_directory( ".fix/pins" );
  if ( exists ) {
    cout << "Reinitialized existing Fix repository in " << std::filesystem::absolute( ".fix" ) << ".\n";
  } else {
    cout << "Initialized empty Fix repository in " << std::filesystem::absolute( ".fix" ) << ".\n";
  }
}

void help( int argc, char* argv[] );

void decode( int argc, char* argv[] )
{
  OptionParser parser( "decode", commands["decode"].second );
  const char* handle = NULL;
  parser.AddArgument(
    "handle", OptionParser::ArgumentCount::One, [&]( const char* argument ) { handle = argument; } );
  parser.Parse( argc, argv );
  if ( !handle )
    exit( EXIT_FAILURE );

  try {
    auto result = base16::decode( handle );
    auto handle = Handle<Fix>::forge( result );
    cout << handle << "\n";
  } catch ( std::runtime_error& e ) {
    cerr << "Error: " << e.what() << "\n";
    exit( EXIT_FAILURE );
  }
}

map<string, pair<function<void( int, char*[] )>, const char*>> commands = {
  { "add", { blob::add, "Add a file to the Fix repository as a Blob." } },
  { "add-blob", { blob::add, "Add a file to the Fix repository as a Blob." } },
  { "cat-blob", { blob::cat, "Print out a Blob." } },
  { "cat-tree", { tree::cat, "Output the contents of a Tree." } },
  { "create-blob", { blob::create, "Create a new Blob." } },
  { "create-tree", { tree::create, "Construct a new Tree." } },
  { "decode", { decode, "Decode a Handle." } },
  { "gc", { gc, "Garbage-collect any data not referenced by a label." } },
  { "help", { help, "Print a list of sub-commands." } },
  { "init", { init, "Initialize a Fix repository in the current directory." } },
  { "label", { label, "Add a label for a Handle." } },
  { "labels", { labels, "List all available labels." } },
  { "ls", { tree::ls, "List the contents of a Tree." } },
  { "ls-tree", { tree::ls, "List the contents of a Tree." } },
  { "eval", { eval, "Eval" } },
};

void help( ostream& os = cout )
{
  os << "fix: available commands\n";
  size_t length = 0;
  for ( const auto& command : commands ) {
    if ( command.first.size() > length )
      length = command.first.size();
  }
  for ( const auto& command : commands ) {
    os << std::format( "    {:{}}    {}\n", command.first, length, command.second.second );
  }
}
void help( int, char*[] )
{
  help();
}

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  google::InitGoogleLogging( argv[0] );
  google::SetStderrLogging( google::INFO );
  google::InstallFailureSignalHandler();

  if ( argc == 1 ) {
    help();
    exit( EXIT_FAILURE );
  }

  for ( const auto& command : commands ) {
    if ( command.first == argv[1] ) {
      try {
        command.second.first( argc - 1, &argv[1] );
        return EXIT_SUCCESS;
      } catch ( const StorageException& e ) {
        cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
      }
    }
  }

  help( cerr );
  return EXIT_FAILURE;
}
