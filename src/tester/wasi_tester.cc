#include <iostream>
#include <string>

#include "base64.hh"
#include "mmap.hh"
#include "name.hh"
#include "option-parser.hh"
#include "runtimestorage.hh"

using namespace std;

// from wasi-libc: sysexits.h
#define EX_OK 0
#define EX_SOFTWARE 70
#define EX_OSERR 71

static bool pretty_print = false;
static const char* s_infile;
static string home_directory_64;
static std::vector<const char*> arguments;

static const char s_description[]
  = R"(./wasi_tester path_to_wasi_wasm_file [options] [base64_home_directory_name] [list_of_arguments])";

static void ParseOptions( int argc, char* argv[] )
{
  OptionParser parser( "wasi_tester", s_description );
  parser.AddOption( "pretty-print", "format exit code to string", []() { pretty_print = true; } );
  parser.AddArgument(
    "filename", OptionParser::ArgumentCount::One, []( const char* argument ) { s_infile = argument; } );
  parser.AddOption( 'h',
                    "home-directory",
                    "HOMEDIR",
                    "base64 encoded name of the home directory for this wasi program",
                    []( const char* argument ) { home_directory_64 = string( argument ); } );
  parser.AddArgument( "arguments", OptionParser::ArgumentCount::ZeroOrMore, []( const char* argument ) {
    arguments.push_back( argument );
  } );
  parser.Parse( argc, argv );
}

static void print_result( int retval )
{
  cout << "The result is ";
  if ( pretty_print ) {
    switch ( retval ) {
      case EX_OK:
        cout << "SUCCESS" << endl;
        break;
      case EX_SOFTWARE:
        cout << "SOFTWARE ERROR (wasi?)" << endl;
        break;
      case EX_OSERR:
        cout << "OS ERROR (flatware?)" << endl;
        break;
      default:
        cout << retval << endl;
        break;
    }
  } else {
    cout << retval << endl;
  }
}

int main( int argc, char* argv[] )
{
  ParseOptions( argc, argv );

  ReadOnlyFile wasm_content { s_infile };

  auto& runtime = RuntimeStorage::get_instance();

  Name wasm_name = runtime.add_blob( string_view( wasm_content ) );

  vector<Name> encode;
  encode.push_back( Name( "empty" ) );
  encode.push_back( wasm_name );

  vector<Name> args;
  for ( const char* argument : arguments ) {
    Name arg_name = runtime.add_blob( string_view( argument, strlen( argument ) + 1 ) );
    args.push_back( arg_name );
  }
  Name args_name = runtime.add_tree( span_view<Name>( args.data(), args.size() ) );
  encode.push_back( args_name );

  if ( home_directory_64.length() != 0 ) {
    runtime.deserialize();
    Name home_dir = base64::decode( home_directory_64 );
    encode.push_back( home_dir );
  }

  Name encode_name = runtime.add_tree( span_view<Name>( encode.data(), encode.size() ) );

  Thunk thunk( encode_name );
  Name thunk_name = runtime.add_thunk( thunk );
  Name res_name = runtime.force_thunk( thunk_name );

  if ( res_name.is_blob() ) {
    cout << dec;
    print_result( *( (const int*)runtime.get_blob( res_name ).data() ) );
    return 0;
  } else {
    auto output_tree = runtime.get_tree( res_name );
    cout << dec;
    print_result( *( (const int*)runtime.get_blob( output_tree[0] ).data() ) );
    auto blob_content = runtime.user_get_blob( output_tree[1] );
    if ( blob_content.size() > 0 ) {
      cout << blob_content.data() << std::endl;
    }
    auto trace_blob = runtime.user_get_blob( output_tree[2] );
    if ( trace_blob.size() > 0 ) {
      cout << "========== FLATWARE TRACE ==========";
      cout << trace_blob.data() << endl;
      cout << "====================================" << endl;
    }
    return 0;
  }
}
