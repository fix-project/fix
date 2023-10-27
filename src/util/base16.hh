#pragma once

#include <cstdint>
#include <immintrin.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace base16 {

static const std::string base16_chars = "0123456789abcdef";

static uint8_t pos_of_char( const unsigned char chr )
{
  if ( chr >= 'A' && chr <= 'Z' )
    return chr - 'A' + 10;
  else if ( chr >= 'a' && chr <= 'z' )
    return chr - 'a' + 10;
  else if ( chr >= '0' && chr <= '9' )
    return chr - '0';
  else
    throw std::runtime_error( "Input is not valid base16-encoded data: contains byte "
                              + std::to_string( (size_t)chr ) );
}

__attribute__( ( unused ) ) static std::string encode( __m256i name_to_encode )
{
  std::string ret;
  ret.reserve( 64 );

  uint8_t name[32];
  _mm256_storeu_si256( reinterpret_cast<__m256i*>( &name[0] ), name_to_encode );

  for ( size_t i = 0; i < 32; i++ ) {
    ret.push_back( base16_chars[name[i] >> 4] );
    ret.push_back( base16_chars[name[i] & 0xf] );
  }

  return ret;
}

__attribute__( ( unused ) ) static __m256i decode( std::string_view encoded_name )
{
  std::string res;
  res.reserve( 32 );

  if ( encoded_name.size() != 64 ) {
    throw std::runtime_error( "Input encoded data is not of length 64." );
  }

  for ( size_t i = 0; i < 32; i++ ) {
    auto x = pos_of_char( encoded_name[2 * i] );
    auto y = pos_of_char( encoded_name[2 * i + 1] );

    res.push_back( x << 4 | y );
  }

  return _mm256_loadu_si256( reinterpret_cast<__m256i*>( res.data() ) );
}

}
