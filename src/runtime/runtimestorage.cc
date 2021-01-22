#include <cassert> 

#include "elfloader.hh"
#include "runtimestorage.hh"
#include "sha256.hh"

using namespace std;

string_view RuntimeStorage::getBlob( const string & name )
{
  return name_to_blob_.get( name ).content();
}

string & RuntimeStorage::getEncodedBlob( const string & name )
{
  return name_to_encoded_blob_.getMutable( name ).content();
}

void RuntimeStorage::addProgram( string& name, vector<string>&& inputs, vector<string>&& outputs, string & program_content )
{
  auto elf_info = load_program( program_content );
  name_to_program_.put( name, link_program( elf_info, name, move( inputs ), move( outputs ) ) );
}

string RuntimeStorage::addEncode( const string & program_name, const vector<string> & input_blobs )
{
  const auto & program = name_to_program_.get( program_name );
  const auto & input_symbols = program.getInputSymbols();
  const auto & output_symbols = program.getOutputSymbols(); 

  assert( input_symbols.size() == input_blobs.size() );
  
  // Construct encode
  map<string, string> input_to_blob;
  for ( size_t i = 0; i < input_symbols.size(); i++ )
  {
    input_to_blob.insert( pair<string, string>( input_symbols[i], input_blobs[i] ));
  }

  Encode encode ( program_name, move( input_to_blob ), output_symbols );
  
  // Construct encoded blobs
  for ( const auto & symbol : output_symbols )
  {
    name_to_encoded_blob_.put( encode.name() + "#" + symbol, EncodedBlob( encode.name(), symbol ) ); 
  }

  string name = encode.name();
  name_to_encode_.put( name, move( encode ) );

  return name;
}

void RuntimeStorage::executeEncode( const string & encode_name )
{
  auto & encode = name_to_encode_.get( encode_name );
  auto & program = name_to_program_.getMutable( encode.getProgramName() );

  // Construct invocation
  uint64_t curr_inv_id = Invocation::next_invocation_id_;
  Invocation::next_invocation_id_++;
  wasi::id_to_inv_.insert( pair<uint64_t, Invocation>( curr_inv_id, Invocation( encode, reinterpret_cast<wasm_rt_memory_t *>( program.getMemLoc() ) ) ) );

  // Set invocation id
  wasi::invocation_id_ = curr_inv_id;
  
  // Execute program
  program.execute();

  // Update encoded_blob to blob

}

  
