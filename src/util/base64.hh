#pragma once

#include <cstdint>
#include <immintrin.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace base64 {

static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                        "abcdefghijklmnopqrstuvwxyz"
                                        "0123456789-_";

static const char trailing_char = '=';

static uint8_t pos_of_char( const unsigned char chr )
{
  if ( chr >= 'A' && chr <= 'Z' )
    return chr - 'A';
  else if ( chr >= 'a' && chr <= 'z' )
    return chr - 'a' + ( 'Z' - 'A' ) + 1;
  else if ( chr >= '0' && chr <= '9' )
    return chr - '0' + ( 'Z' - 'A' ) + ( 'z' - 'a' ) + 2;
  else if ( chr == '-' )
    return 62;
  else if ( chr == '_' )
    return 63;
  else
    throw std::runtime_error( "Input is not valid base64-encoded data." );
}

__attribute__( ( unused ) ) static std::string encode( __m256i name_to_encode )
{
  std::string ret;
  ret.reserve( 43 );

  uint8_t name[32];
  _mm256_storeu_si256( reinterpret_cast<__m256i*>( &name[0] ), name_to_encode );

  uint8_t pos = 0;
  while ( pos < 32 ) {
    ret.push_back( base64_chars[( name[pos] & 0xfc ) >> 2] );
    if ( pos + 1 < 32 ) {
      ret.push_back( base64_chars[( ( name[pos] & 0x03 ) << 4 ) + ( ( name[pos + 1] & 0xf0 ) >> 4 )] );
      if ( pos + 2 < 32 ) {
        ret.push_back( base64_chars[( ( name[pos + 1] & 0x0f ) << 2 ) + ( ( name[pos + 2] & 0xc0 ) >> 6 )] );
        ret.push_back( base64_chars[( name[pos + 2] & 0x3f )] );
      } else {
        ret.push_back( base64_chars[( ( name[pos + 1] & 0x0f ) << 2 )] );
      }
    } else {
      throw std::runtime_error( "Input is not of length 32." );
    }

    pos += 3;
  }

  return ret;
}

__attribute__( ( unused ) ) static __m256i decode( std::string_view encoded_name )
{
  std::string res;
  res.reserve( 32 );

  if ( encoded_name.size() != 43 ) {
    throw std::runtime_error( "Input encoded data is not of length 43." );
  }

  uint8_t pos = 0;
  while ( pos < 40 ) {
    auto pos_of_char_0 = pos_of_char( encoded_name[pos] );
    auto pos_of_char_1 = pos_of_char( encoded_name[pos + 1] );
    auto pos_of_char_2 = pos_of_char( encoded_name[pos + 2] );
    auto pos_of_char_3 = pos_of_char( encoded_name[pos + 3] );
    res.push_back( ( pos_of_char_0 << 2 ) + ( ( pos_of_char_1 & 0x30 ) >> 4 ) );
    res.push_back( ( ( pos_of_char_1 & 0x0f ) << 4 ) + ( ( pos_of_char_2 & 0x3c ) >> 2 ) );
    res.push_back( ( ( pos_of_char_2 & 0x03 ) << 6 ) + pos_of_char_3 );

    pos += 4;
  }

  auto pos_of_char_40 = pos_of_char( encoded_name[40] );
  auto pos_of_char_41 = pos_of_char( encoded_name[41] );
  auto pos_of_char_42 = pos_of_char( encoded_name[42] );
  res.push_back( ( pos_of_char_40 << 2 ) + ( ( pos_of_char_41 & 0x30 ) >> 4 ) );
  res.push_back( ( ( pos_of_char_41 & 0x0f ) << 4 ) + ( ( pos_of_char_42 & 0x3c ) >> 2 ) );

  return _mm256_loadu_si256( reinterpret_cast<__m256i*>( res.data() ) );
}

}
