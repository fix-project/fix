#include <iostream>
#include <string>

#include "base64.hh"
#include "mmap.hh"
#include "option-parser.hh"
#include "runtimestorage.hh"

using namespace std;

// from wasi-libc: sysexits.h
#define EX_OK 0
#define EX_SOFTWARE 70
#define EX_OSERR 71

#define TRUSTED_COMPILE_ENCODE Handle( base64::decode( COMPILE_ENCODE ) )

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
  if ( retval != 0 ) {
    throw std::runtime_error( "Test case returned a non zero value" );
  }
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

  Handle wasm_name = runtime.add_blob( string_view( wasm_content ) );

  Tree wasm_compile { 3 };
  wasm_compile.at( 0 ) = Handle( "unused" );
  wasm_compile.at( 1 ) = TRUSTED_COMPILE_ENCODE;
  wasm_compile.at( 2 ) = wasm_name;
  Handle compile_encode_tree = runtime.add_tree( std::move( wasm_compile ) );

  Thunk wasm_compile_thunk( compile_encode_tree );
  Handle wasm_compile_thunk_name = runtime.add_thunk( wasm_compile_thunk );

  vector<Handle> encode;
  encode.push_back( Handle( "empty" ) );
  encode.push_back( wasm_compile_thunk_name );

  vector<Handle> args;
  for ( const char* argument : arguments ) {
    Handle arg_name = runtime.add_blob( string_view( argument, strlen( argument ) + 1 ) );
    args.push_back( arg_name );
  }
  Handle args_name = runtime.add_tree( span_view<Handle>( args.data(), args.size() ) );
  encode.push_back( args_name );

  if ( home_directory_64.length() != 0 ) {
    runtime.deserialize();
    Handle home_dir = base64::decode( home_directory_64 );
    encode.push_back( home_dir );
  }

  Handle encode_name = runtime.add_tree( span_view<Handle>( encode.data(), encode.size() ) );

  Thunk thunk( encode_name );
  Handle thunk_name = runtime.add_thunk( thunk );
  Handle res_name = runtime.eval_thunk( thunk_name );

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
