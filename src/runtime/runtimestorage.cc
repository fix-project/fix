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

template<class T>
void RuntimeStorage::addBlob( T&& content )
{
  string blob_content ( reinterpret_cast<char *>( content ), sizeof( T ) );
  Blob blob ( move( blob_content ) );
  name_to_blob_.put( blob.name(), move( blob ) );
}

void RuntimeStorage::addProgram( string& name, vector<string>&& inputs, vector<string>&& outputs, string & program_content )
{
  auto elf_info = load_program( program_content );
  name_to_program_.put( name, link_program( elf_info, name, move( inputs ), move( outputs ) ) );
}

void RuntimeStorage::addEncode( const string & program_name, const vector<string> & input_blobs )
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

  name_to_encode_.put( encode.name(), move( encode ) );
}
  
