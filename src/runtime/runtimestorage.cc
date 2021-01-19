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
void RuntimeStorage::addBlob( T content )
{
  string blob_content ( reinterpret_cast<char *>( content ), sizeof( T ) );
  auto name = sha256::encode( blob_content );
  name_to_blob_.put( name, Blob( name, blob_content ) );
}

void RuntimeStorage::addProgram( string& name, vector<string>&& inputs, vector<string>&& outputs, string & program_content )
{
  auto elf_info = load_program( program_content );
  name_to_program_.put( name, link_program( elf_info, name, move( inputs ), move( outputs ) ) );
}
