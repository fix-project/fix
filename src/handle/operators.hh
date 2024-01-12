#pragma once
#include <ostream>

#include "types.hh"

#ifdef __AVX__
#include <immintrin.h>
#endif

template<typename T>
inline bool operator==( const Handle<T>& lhs, const Handle<T>& rhs )
{
  u64x4 pxor = (u64x4)( lhs.content ^ rhs.content );
#ifdef __AVX__
  return _mm256_testz_si256( (__m256i)pxor, (__m256i)pxor );
#else
  return ( xored[0] | xored[1] | xored[2] | xored[3] ) == 0;
#endif
}

template<typename T>
inline std::ostream& operator<<( std::ostream& os, const Handle<T>& h )
{
  char buf[65];
  for ( unsigned i = 0; i < 32; i++ ) {
    snprintf( &buf[2 * i], 3, "%02x", h.content[i] );
  }
  os << std::string( buf );
  return os;
}
