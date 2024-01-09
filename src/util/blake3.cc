#include "sha256.hh"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#include "blake3.h"
#include "timer.hh"

using namespace std;

namespace blake3 {
std::string hash( const std::string& input )
{
  blake3_hasher hasher;
  blake3_hasher_init( &hasher );

  blake3_hasher_update( &hasher, input.data(), input.size() );

  uint8_t hash_buf[BLAKE3_OUT_LEN];
  blake3_hasher_finalize( &hasher, hash_buf, BLAKE3_OUT_LEN );

  // Convert the hash to a hexadecimal string
  std::stringstream ss;
  ss << std::hex << std::setfill( '0' );
  for ( size_t i = 0; i < BLAKE3_OUT_LEN; ++i ) {
    ss << std::setw( 2 ) << static_cast<unsigned>( hash_buf[i] );
  }

  return ss.str();
}

}
