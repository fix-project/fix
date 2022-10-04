#pragma once

#include "cookie.hh"
#include "name.hh"

/**
 * ObjectReference metadata:
 * | accessible/not accessible | other/literal | 0 | the same as underlying name
 */
class ObjectReference : public cookie_name
{
public:
  ObjectReference() = default;

  ObjectReference( const __m256i val )
    : cookie_name( val )
  {
  }

  operator __m256i() const { return content_; }

  bool is_accessible() const { return !( metadata() & 0x80 ); }

  static ObjectReference get_object_reference( Name name, bool accessible )
  {
    __m256i name_only = Name::name_only( name );
    if ( !accessible ) {
      __m256i mask = _mm256_set_epi64x( 0x80'00'00'00'00'00'00'00, 0, 0, 0 );
      return _mm256_or_si256( mask, name );
    } else {
      return name_only;
    }
  }

  static Name object_reference_name_only( ObjectReference ref )
  {
    __m256i mask = _mm256_set_epi64x( 0xa0'00'00'00'00'00'00'00, 0, 0, 0 );
    return _mm256_andnot_si256( mask, ref );
  }
};
