#include "blake3.hh"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#include "blake3.h"
#include "timer.hh"
#include <cstring>

using namespace std;

namespace blake3 {
std::string encode( const std::string_view input )
{
  blake3_hasher hasher;
  blake3_hasher_init( &hasher );
  blake3_hasher_update( &hasher, input.data(), input.size() );
  string digest( BLAKE3_OUT_LEN, '\0' );
  blake3_hasher_finalize( &hasher, reinterpret_cast<uint8_t*>( digest.data() ), BLAKE3_OUT_LEN );
  return digest;
}
}
