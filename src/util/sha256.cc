#include "sha256.hh"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#include "blake3.h"
#include "timer.hh"

using namespace std;

namespace sha256 {
string encode( std::string_view input )
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    uint length = input.length();

    char digest[2 * BLAKE3_OUT_LEN + 1];
    uint8_t hash_buf[BLAKE3_OUT_LEN];

    blake3_hasher_update(&hasher, input.data(), length);
    blake3_hasher_finalize(&hasher, hash_buf, BLAKE3_OUT_LEN);

    // Convert the hash buffer hexadecimal to a string
    for (size_t i = 0; i < BLAKE3_OUT_LEN; i++) {
	    sprintf(&digest[i * 2],"%02x", hash_buf[i]);
    }
    digest[2 * BLAKE3_OUT_LEN] = '\0';
    return digest;
}
}
