#include "base64.hh"
#include "handle.hh"
#include "sha256.hh"

#include <iostream>
#include <stdlib.h>

#define ITERATION 1000

using namespace std;

bool test()
{
  int x = rand();
  string hash_x = sha256::encode( { reinterpret_cast<char*>( &x ), sizeof( int ) } );
  Handle name_x( hash_x, x, ContentType::Blob );
  string base64_encoded = base64::encode( name_x );
  Handle base64_decoded_name_x = base64::decode( base64_encoded );
  return ( base64_decoded_name_x == name_x );
}

int main()
{
  for ( int i = 0; i < ITERATION; i++ ) {
    if ( !test() ) {
      cerr << "Unmatched base64 result.\n";
      return 1;
    }
  }

  return 0;
}
