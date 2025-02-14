#pragma once
#include <algorithm>
#include <cstdint>
#include <format>
#include <ostream>

#include "blob.hh"
#include "tree.hh"
#include "treeref.hh"
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

inline bool equal( const u8x32& lhs, const u8x32& rhs )
{
  u64x4 pxor = (u64x4)( lhs ^ rhs );
#ifdef __AVX__
  return _mm256_testz_si256( (__m256i)pxor, (__m256i)pxor );
#else
  return ( xored[0] | xored[1] | xored[2] | xored[3] ) == 0;
#endif
}

inline bool content_equal( const u8x32& lhs, const u8x32& rhs )
{
  static constexpr u64x4 mask = { 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffff000000000000 };
  u64x4 pxor = (u64x4)( lhs ^ rhs ) & mask;
#ifdef __AVX__
  return _mm256_testz_si256( (__m256i)pxor, (__m256i)pxor );
#else
  return ( xored[0] | xored[1] | xored[2] | xored[3] ) == 0;
#endif
}

inline std::ostream& operator<<( std::ostream& os, const u8x32& v )
{
  char buf[65];
  for ( unsigned i = 0; i < 32; i++ ) {
    snprintf( &buf[2 * i], 3, "%02x", v[i] );
  }
  os << std::string( buf );
  return os;
}

inline std::ostream& operator<<( std::ostream& os, const Handle<Named>& h )
{
  os << "Named size:" << h.size() << " name:";
  if ( h.is_local() ) {
    os << "(Local " << h.local_name() << ")";
  } else {
    os << "(Canonical " << h.hash() << ")";
  }
  return os;
}

inline std::ostream& operator<<( std::ostream& os, const Handle<Literal>& h )
{
  os << "Literal size:" << (size_t)h.size() << " contents:";

  auto s = std::string( h );
  bool is_printable
    = (unsigned)std::count_if( s.begin(), s.end(), []( unsigned char c ) { return std::isprint( c ); } )
      == s.size();

  if ( is_printable ) {
    os << '"';
    os << s;
    os << '"';
  } else if ( h.size() == 8 ) {
    os << std::format( "0x{:016x}", uint64_t( h ) );
  } else if ( h.size() == 4 ) {
    os << std::format( "0x{:08x}", uint32_t( h ) );
  } else if ( h.size() == 2 ) {
    os << std::format( "0x{:04x}", uint16_t( h ) );
  } else if ( h.size() == 1 ) {
    os << std::format( "0x{:02x}", uint8_t( h ) );
  } else {
    os << "[";
    for ( int i = 0; i < h.size() - 1; i++ ) {
      os << std::format( "0x{:02x}, ", h.content[i] );
    }
    os << std::format( "0x{:02x}", h.content[h.size() - 1] );
    os << "]";
  }
  return os;
}

inline std::ostream& operator<<( std::ostream& os, const Handle<Tree>& h )
{
  os << "Tree " << h.size() << " ";
  if ( h.is_local() ) {
    os << "(Local " << h.local_name() << ")";
  } else {
    os << "(Canonical " << h.hash() << ")";
  }
  return os;
}

inline std::ostream& operator<<( std::ostream& os, const Handle<TreeRef>& h )
{
  os << "TreeRef " << h.size() << " ";
  if ( h.is_local() ) {
    os << "(Local " << h.local_name() << ")";
  } else {
    os << "(Canonical " << h.hash() << ")";
  }
  return os;
}

inline std::ostream& operator<<( std::ostream& os, const Handle<Tag>& h )
{
  os << "Tag " << h.size() << " ";
  if ( h.is_local() ) {
    os << "(Local " << h.local_name() << ")";
  } else {
    os << "(Canonical " << h.hash() << ")";
  }
  return os;
}

inline std::ostream& operator<<( std::ostream& os, const Handle<TagRef>& h )
{
  os << "TagRef " << h.size() << " ";
  if ( h.is_local() ) {
    os << "(Local " << h.local_name() << ")";
  } else {
    os << "(Canonical " << h.hash() << ")";
  }
  return os;
}

namespace std {
template<typename T>
struct hash<Handle<T>>
{
  size_t operator()( const Handle<T>& x ) const
  {
    u64x4 dwords = (u64x4)x.content;
    return hash<uint64_t>()( dwords[0] ) ^ hash<uint64_t>()( dwords[1] ) ^ hash<uint64_t>()( dwords[2] );
  }
};
}
