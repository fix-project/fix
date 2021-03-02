#include <cassert> 

#include "elfloader.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "timing_helper.hh"

using namespace std;

string_view RuntimeStorage::getBlob( const string & name )
{
  if ( encoded_blob_to_blob_.contains( name ) )
  {
    string blob_name = encoded_blob_to_blob_.at( name );
    return name_to_blob_.get( blob_name ).content();
  } else {
    return name_to_blob_.get( name ).content();
  }
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
  absl::flat_hash_map<string, string> input_to_blob;
  for ( size_t i = 0; i < input_symbols.size(); i++ )
  {
    input_to_blob.try_emplace( input_symbols[i], input_blobs[i] );
  }

  Encode encode ( program_name, std::move( input_to_blob ), output_symbols );
  
  // Construct encoded blobs
  for ( const auto & symbol : output_symbols )
  {
    name_to_encoded_blob_.put( encode.name() + "#" + symbol, EncodedBlob( encode.name(), symbol ) ); 
  }

  string name = encode.name();
  name_to_encode_.put( name, move( encode ) );

  return name;
}

void RuntimeStorage::executeEncode( const string & encode_name, int arg1, int arg2 )
{
  auto & encode = name_to_encode_.get( encode_name );
  auto & program = name_to_program_.getMutable( encode.getProgramName() );
   
  // Construct invocation
 
  uint64_t curr_inv_id = 0; 
  {
    RecordScopeTimer<Timer::Category::Nonblock> record_timer { _pre_execution };
    curr_inv_id = Invocation::next_invocation_id_;
    Invocation::next_invocation_id_++;
    wasi::id_to_inv_.try_emplace( curr_inv_id, Invocation( encode, reinterpret_cast<wasm_rt_memory_t *>( program.getMemLoc() ) ) );

    // Set invocation id
    wasi::invocation_id_ = curr_inv_id;
  }

  // Execute program
  program.execute( arg1, arg2 );

  RecordScopeTimer<Timer::Category::Nonblock> record_timer { _post_execution };
  // Update encoded_blob to blob
  for ( const auto & [ varaible, encoded_blob_name ] : encode.getOutputBlobNames() )
  {
    Blob output ( move( name_to_encoded_blob_.getMutable( encoded_blob_name ).content()) );
    encoded_blob_to_blob_.try_emplace( encoded_blob_name, output.name() );
    name_to_blob_.put( output.name() , move( output ) );
  }

  wasi::id_to_inv_.erase( curr_inv_id );

}

  
