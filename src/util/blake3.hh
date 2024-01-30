#pragma once

#include "types.hh"
#include <immintrin.h>
#include <string_view>

namespace blake3 {
void encode( std::string_view input, u8x32& output );
}
