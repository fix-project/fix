#include <string_view> 

#include "sha256.hh" 

#include "encode.hh"

using namespace std;

Encode::Encode( const string & program_name, map<string, string> && input_to_blob, const vector<string> & output_symbols ) 
  : name_(),
    program_name_( program_name ),
    input_to_blob_( move( input_to_blob ) ),
    output_to_blob_()
{
  string_view encode ( reinterpret_cast<const char *>( this ), sizeof( Encode ) );
  name_ = sha256::encode( encode );

  for ( const auto & symbol : output_symbols )
  {
    output_to_blob_.insert( pair<string, string>( symbol, name_ + "#" + symbol ) );
  }
}

