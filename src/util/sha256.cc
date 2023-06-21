#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>

#include "sha256.hh"
#include "timer.hh"

using namespace std;
namespace CryptoPP {
using byte = unsigned char;
}

string sha256::encode( string_view input )
{
  GlobalScopeTimer<Timer::Category::Hash> record_timer;
  CryptoPP::SHA256 hash;

  CryptoPP::byte digest[CryptoPP::SHA256::DIGESTSIZE];
  hash.CalculateDigest( digest, (const CryptoPP::byte*)input.data(), input.size() );

  return string( (char*)digest, CryptoPP::SHA256::DIGESTSIZE );
}

bool sha256::verify( const string& ret, const string& input )
{
  GlobalScopeTimer<Timer::Category::Hash> record_timer;
  CryptoPP::SHA256 hash;
  bool verified;

  hash.Update( reinterpret_cast<const unsigned char*>( input.data() ), input.size() );
  verified = hash.Verify( reinterpret_cast<const unsigned char*>( ret.data() ) );

  return verified;
}
