#pragma once

#include "types.hh"
#include <immintrin.h>
#include <span>
#include <string_view>

namespace blake3 {
u8x32 encode( std::span<const std::byte> );
}
