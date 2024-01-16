#include "blake3.hh"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#include "blake3.h"
#include "timer.hh"

using namespace std;

namespace blake3 {
std::string encode( const std::string_view input )
{
  blake3_hasher hasher;
  blake3_hasher_init( &hasher );

  blake3_hasher_update( &hasher, input.data(), input.size() );

  uint8_t hash_buf[BLAKE3_OUT_LEN];
  blake3_hasher_finalize( &hasher, hash_buf, BLAKE3_OUT_LEN );
  return std::string( (char*)hash_buf, BLAKE3_OUT_LEN );
}
}
