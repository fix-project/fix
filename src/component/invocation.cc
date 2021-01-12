#include "invocation.hh"

using namespace std;

bool Invocation::isInput( const string & variable_name )
{
  auto it = input_to_blob_.find( variable_name );
  return it != input_to_blob_.end();
}

string Invocation::getInputBlobName( const string & variable_name )
{
  return input_to_blob_.at( variable_name );
}

string Invocation::getOutputBlobName( const string & variable_name )
{
  return output_to_blob_.at( variable_name );
}
