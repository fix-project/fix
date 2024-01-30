#include "blake3.hh"
#include "blake3.h"
#include <array>
#include <cstring>

using namespace std;

namespace blake3 {
void encode( std::string_view input, u8x32& output )
{
  array<uint8_t, BLAKE3_OUT_LEN> tmp;
  blake3_hasher hasher;
  blake3_hasher_init( &hasher );
  blake3_hasher_update( &hasher, input.data(), input.size() );
  blake3_hasher_finalize( &hasher, tmp.data(), BLAKE3_OUT_LEN );
  memcpy( &output, tmp.data(), BLAKE3_OUT_LEN );
}
}
