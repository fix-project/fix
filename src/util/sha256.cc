#include <crypto++/filters.h>
#include <crypto++/hex.h>
#include <crypto++/sha.h>

#include "sha256.hh"

using namespace CryptoPP;
using namespace std;

string sha256::encode( const string & input )
{
  SHA256 hash;
  string ret;

  StringSource s( input, true,
                  new HashFilter( hash, 
                    new HexEncoder( new StringSink( ret ) ) ) );

  return ret;
}

bool sha256::verify( const string & ret, const string & input )
{
  SHA256 hash;
  bool verified;

  hash.Update( reinterpret_cast<const unsigned char*>( input.data() ), input.size() );
  verified = hash.Verify( reinterpret_cast<const unsigned char*>( ret.data() ) );

  return verified;
} 
  
