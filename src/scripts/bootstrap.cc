#include <fstream>

#include "base64.hh"
#include "mmap.hh"
#include "option-parser.hh"
#include "runtimestorage.hh"

using namespace std;

static string boot_path;

static const char s_description[] = R"(./bootstrap path_to_boot)";

static void ParseOptions( int argc, char* argv[] )
{
  OptionParser parser( "bootstrap", s_description );

  parser.AddArgument( "path_to_boot", OptionParser::ArgumentCount::One, []( const char* argument ) {
    boot_path = string( argument );
  } );

  parser.Parse( argc, argv );
}

int main( int argc, char* argv[] )
{
  ParseOptions( argc, argv );

  auto& runtime = RuntimeStorage::get_instance();
  runtime.deserialize();

  array<std::string, 5> tasks = { "wasm-to-c-fix", "c-to-elf-fix", "link-elfs-fix", "compile", "map" };
  vector<Handle> wasm_names;
  for ( auto task : tasks ) {
    ReadOnlyFile wasm_file { boot_path + "/" + task + "-wasm" };
    wasm_names.push_back( base64::decode( string( wasm_file ) ) );
  }
  ReadOnlyFile compile_tool { boot_path + "/compile-tool" };
  Handle compile_tool_name( base64::decode( string( compile_tool ) ) );

  // Apply compile-tool to each wasm
  vector<Handle> new_elf_names;
  size_t index = 0;
  for ( auto task : tasks ) {
    vector<Handle> compile_encode;
    compile_encode.push_back( Handle( "empty" ) );
    compile_encode.push_back( compile_tool_name );
    compile_encode.push_back( wasm_names[index] );
    index++;
    Handle compile_encode_name
      = runtime.add_tree( span_view<Handle>( compile_encode.data(), compile_encode.size() ) );

    Thunk compile_thunk( compile_encode_name );
    Handle compile_thunk_name = runtime.add_thunk( compile_thunk );
    Handle output_elf_name = runtime.eval_thunk( compile_thunk_name );
    new_elf_names.push_back( output_elf_name );
  }

  index = 0;
  for ( auto task : tasks ) {
    ofstream fout( boot_path + "/" + task + "-elf-new" );
    fout << runtime.serialize( new_elf_names.at( index ) );
    index++;
    fout.close();
  }

  return 0;
}
