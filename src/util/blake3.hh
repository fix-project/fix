#pragma once

#include <immintrin.h>
#include <string_view>

namespace blake3 {
void encode( std::string_view input, __m256i& output );
}
