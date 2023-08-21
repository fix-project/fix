#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unistd.h>
#include <variant>
#include <worker.cc>

#include "operation.hh"
#include "tester-utils.hh"

using namespace std;

bool try_print_as_string( string_view& blob )
{
  bool printable = true;
  for ( const auto& c : blob ) {
    if ( !isprint( c ) ) {
      printable = false;
      break;
    }
  }
  if ( printable ) {
    cout << blob;
    return true;
  }
  return false;
}

void print_string_view( string_view&& blob )
{
  if ( !try_print_as_string( blob ) ) {
    for ( size_t i = 0; i < std::min( blob.size(), (size_t)32 ); i++ ) {
      const char& byte = blob[blob.size() - i - 1];
      cout << hex << setfill( '0' ) << setw( 2 ) << (int)byte;
    }
    if ( blob.size() > 32 ) {
      cout << "...";
    }
  }
}

int parse_u64( string id )
{
  return stoul( id, nullptr, 16 );
}

optional<Handle> parse_handle( string id )
{
  string delimiter = "|";
  size_t pos;
  vector<uint64_t> numbers;
  for ( size_t i = 0; i < 4; i++ ) {
    pos = id.find( delimiter );
    if ( pos == string::npos ) {
      numbers.push_back( stoul( id, nullptr, 16 ) );
      break;
    }
    numbers.push_back( stoul( id.substr( 0, pos ), nullptr, 16 ) );
    id = id.substr( pos + 1 );
  }
  if ( numbers.size() != 4 ) {
    return {};
  }
  return Handle( numbers[0], numbers[1], numbers[2], numbers[3] );
}

void program_body( span_view<char*> args )
{
  ios::sync_with_stdio( false );
  vector<ReadOnlyFile> open_files;

  args.remove_prefix( 1 ); // ignore argv[ 0 ]

  // make the combination from the given arguments
  Handle encode_name = parse_args( args, open_files );

  if ( not args.empty() ) {
    throw runtime_error( "unexpected argument: "s + args.at( 0 ) );
  }

  // add the combination to the store, and print it
  cout << "Combination:\n" << pretty_print( encode_name ) << "\n";

  auto& runtime = RuntimeStorage::get_instance();

  // make a Thunk that points to the combination
  Handle thunk_name = runtime.add_thunk( Thunk { encode_name } );

  // force the Thunk and print it
  Handle result = runtime.eval_thunk( thunk_name );

  // print the result
  cout << "Result:\n" << pretty_print( result ) << endl;

  cout << "Initial thunk_name: " << thunk_name << std::endl;
  cout << "Result handle: " << result << endl;
  cout << endl;

  while ( true ) {
    cout << "Enter [parents(p)|content(c)|dependees(d)] and a local id or handle in form ab|cd|ef|01 (empty to "
            "quit):"
         << endl;
    string input;
    getline( cin, input );
    if ( input.length() == 0 ) {
      break;
    }
    size_t pos = input.find( " " );

    if ( pos == string::npos ) {
      cout << "Put a space between [parents(p)|content|dependees] and [local-id|handle]" << endl;
      continue;
    }

    string command = input.substr( 0, pos );
    string id_input = input.substr( pos + 1 );

    if ( command == "parents" || command == "p" ) {
      auto input = parse_handle( id_input );
      if ( !input.has_value() ) {
        cout << "Could not parse handle (parents command must take a handle)." << endl;
        continue;
      }
      cout << "[\n";
      for ( auto& task : runtime.get_parents( input.value() ) ) {
        cout << "  " << task << ",\n";
      }
      cout << "]\n";
    } else if ( command == "content" || command == "c" ) {

      size_t id;
      if ( id_input.find( "|" ) == string::npos ) {
        id = stoul( id_input, nullptr, 16 );
      } else {
        auto input = parse_handle( id_input );
        if ( !input.has_value() ) {
          cout << "Could not parse handle." << endl;
          continue;
        }
        auto handle = input.value();
        if ( handle.is_local() ) {
          id = handle.get_local_id();
        } else if ( handle.is_canonical() ) {
          id = runtime.get_local_handle( handle ).get_local_id();
        } else {
          cout << "Blob: ";
          print_string_view( handle.literal_blob() );
          cout << endl;
          continue;
        }
      }

      if ( id >= runtime.get_local_storage_size() ) {
        cout << "Entry does not exist in local storage." << endl;
        continue;
      }
      if ( holds_alternative<Blob>( runtime.local_storage_at( id ) ) ) {
        cout << "Blob: ";
        print_string_view( get<Blob>( runtime.local_storage_at( id ) ) );
        cout << endl;
      } else {
        cout << "Tree: [" << endl;
        span_view<Handle> entries = get<Tree>( runtime.local_storage_at( id ) );
        for ( auto& entry : entries ) {
          cout << "  " << entry;
          if ( entry.is_literal_blob() ) {
            cout << " = ";
            print_string_view( entry.literal_blob() );
          }
          cout << "," << endl;
        }
        cout << "]" << endl;
      }
    } else if ( command == "dependees" || command == "d" ) {
      auto input = parse_handle( id_input );
      if ( !input.has_value() ) {
        cout << "Could not parse handle (dependees command must take a handle)." << endl;
        continue;
      }
      cout << "Apply dependees: [" << endl;
      vector<Task> apply_entries = runtime.get_dependees( Task( input.value(), Operation::Apply ) );
      for ( auto& entry : apply_entries ) {
        cout << "  " << entry;
        if ( entry.handle().is_literal_blob() ) {
          cout << " = ";
          print_string_view( entry.handle().literal_blob() );
        }
        cout << "," << endl;
      }
      cout << "]" << endl;

      cout << "Eval dependees: [" << endl;
      vector<Task> eval_entries = runtime.get_dependees( Task( input.value(), Operation::Eval ) );
      for ( auto& entry : eval_entries ) {
        cout << "  " << entry;
        if ( entry.handle().is_literal_blob() ) {
          cout << " = ";
          print_string_view( entry.handle().literal_blob() );
        }
        cout << "," << endl;
      }
      cout << "]" << endl;

      cout << "Fill dependees: [" << endl;
      vector<Task> fill_entries = runtime.get_dependees( Task( input.value(), Operation::Fill ) );
      for ( auto& entry : fill_entries ) {
        cout << "  " << entry;
        if ( entry.handle().is_literal_blob() ) {
          cout << " = ";
          print_string_view( entry.handle().literal_blob() );
        }
        cout << "," << endl;
      }
      cout << "]" << endl;

    } else {
      cout << "Enter parent, entries, or dependees as the first word";
      continue;
    }
  }
}

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " entry...\n";
  cerr << "   entry :=   file:<filename>\n";
  cerr << "            | string:<string>\n";
  cerr << "            | name:<base64-encoded name>\n";
  cerr << "            | uint<n>:<integer> (with <n> = 8 | 16 | 32 | 64)\n";
  cerr << "            | tree:<n> (followed by <n> entries)\n";
  cerr << "            | thunk: (followed by tree:<n>)\n";
  cerr << "            | compile:<filename>\n";
}

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  try {
    if ( argc < 2 ) {
      usage_message( argv[0] );
      return EXIT_FAILURE;
    }

    span_view<char*> args = { argv, static_cast<size_t>( argc ) };
    program_body( args );
  } catch ( const exception& e ) {
    cerr << argv[0] << ": " << e.what() << "\n\n";
    usage_message( argv[0] );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
