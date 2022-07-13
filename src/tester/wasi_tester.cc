#include <iostream>
#include <string>

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
static std::vector<const char*> arguments;

static const char s_description[] = R"(./wasi_tester path_to_wasi_wasm_file [options] [list_of_arguments])";

static void ParseOptions( int argc, char* argv[] )
{
  OptionParser parser( "wasi_tester", s_description );
  parser.AddOption( "pretty-print", "format exit code to string", []() { pretty_print = true; } );
  parser.AddArgument(
    "filename", OptionParser::ArgumentCount::One, []( const char* argument ) { s_infile = argument; } );
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

  for ( const char* argument : arguments ) {
    Name arg_name = runtime.add_blob( string_view( argument, strlen( argument ) + 1 ) );
    encode.push_back( arg_name );
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
    return 0;
  }
}
