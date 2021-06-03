#include <crypto++/filters.h>
#include <crypto++/hex.h>
#include <crypto++/sha.h>

#include "sha256.hh"
#include "timing_helper.hh"

using namespace std;
namespace CryptoPP {
using byte = unsigned char;
}

string sha256::encode( const string& input )
{

  //  RecordScopeTimer<Timer::Category::Nonblock> record_timer { _hash };
  CryptoPP::SHA256 hash;

  CryptoPP::byte digest[CryptoPP::SHA256::DIGESTSIZE];
  hash.CalculateDigest( digest, (const CryptoPP::byte*)input.c_str(), input.length() );

  return string( (char*)digest, CryptoPP::SHA256::DIGESTSIZE );
}

string sha256::encode( string_view input )
{
  RecordScopeTimer<Timer::Category::Nonblock> record_timer { _hash };
  CryptoPP::SHA256 hash;

  CryptoPP::byte digest[CryptoPP::SHA256::DIGESTSIZE];
  hash.CalculateDigest( digest, (const CryptoPP::byte*)input.data(), input.size() );

  return string( (char*)digest, CryptoPP::SHA256::DIGESTSIZE );
}

bool sha256::verify( const string& ret, const string& input )
{
  CryptoPP::SHA256 hash;
  bool verified;

  hash.Update( reinterpret_cast<const unsigned char*>( input.data() ), input.size() );
  verified = hash.Verify( reinterpret_cast<const unsigned char*>( ret.data() ) );

  return verified;
}
