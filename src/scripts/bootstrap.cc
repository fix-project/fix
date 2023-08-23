#include <fstream>

#include "base64.hh"
#include "mmap.hh"
#include "option-parser.hh"
#include "runtimestorage.hh"

using namespace std;

static string boot_path;

static const char s_description[] = R"(./bootstrap)";

static void ParseOptions( int argc, char* argv[] )
{
  OptionParser parser( "bootstrap", s_description );

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
    wasm_names.push_back( *runtime.get_ref( task + "-wasm" ) );
  }
  Handle compile_encode_name( *runtime.get_ref( "compile-encode" ) );

  // Apply compile-encode to each wasm
  vector<Handle> new_elf_names;
  size_t index = 0;
  for ( auto task : tasks ) {
    vector<Handle> compile_encode;
    compile_encode.push_back( Handle( "empty" ) );
    compile_encode.push_back( compile_encode_name );
    compile_encode.push_back( wasm_names[index] );
    cout << "Compiling " << runtime.get_display_name( wasm_names[index] ) << endl;
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
