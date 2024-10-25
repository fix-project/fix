#pragma once
#include <stdint.h>

#include "blob.hh"
#include "types.hh"

inline auto operator""_literal( const char* str, unsigned long n )
{
  return Handle<Literal>( { str, n } );
}

inline auto operator""_literal( char c )
{
  return Handle<Literal>( (uint8_t)c );
}

inline auto operator""_literal8( unsigned long long x )
{
  return Handle<Literal>( (uint8_t)x );
}

inline auto operator""_literal16( unsigned long long x )
{
  return Handle<Literal>( (uint16_t)x );
}

inline auto operator""_literal32( unsigned long long x )
{
  return Handle<Literal>( (uint32_t)x );
}

inline auto operator""_literal64( unsigned long long x )
{
  return Handle<Literal>( (uint64_t)x );
}

inline auto operator""_literal32( long double x )
{
  return Handle<Literal>( (float)x );
}

inline auto operator""_literal64( long double x )
{
  return Handle<Literal>( (double)x );
}
