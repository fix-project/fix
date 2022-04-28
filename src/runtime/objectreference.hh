#pragma once

#include "cookie.hh"
#include "name.hh"

/**
 * ObjectReference metadata:
 * | reffed/not reffed | other/literal | accessible/not assessible | the same as underlying name
 */
class ObjectReference : public cookie_name 
{
public:
  ObjectReference() = default;

  ObjectReference( const __m256i val )
    : cookie_name( val )
  {}
  
  operator __m256i() const { return content_; }
  
  bool is_reffed() const { return ( metadata() & 0x80 ); }

  bool is_accessible() const { return !( metadata() & 0x20 ); }

  static ObjectReference get_object_reference( Name name, bool accessible )
  {
    __m256i name_only = Name::name_only( name );
    if ( !accessible ) {
      return __m256i { name_only[0],
                       name_only[1],
                       name_only[2],
                       name_only[3] | 0x20'00'00'00'00'00'00'00 };
    } else {
      return name_only;
    }
  }

  static Name object_reference_name_only( ObjectReference ref )
  {
    return __m256i {
      ref.content_[0], ref.content_[1], ref.content_[2], ref.content_[3] & 0x5f'ff'ff'ff'ff'ff'ff'ff
    };
  }

  static ObjectReference ref_object_reference( ObjectReference ref )
  {
    return __m256i { ref.content_[0],
                     ref.content_[1],
                     ref.content_[2],
                     static_cast<int64_t>( ref.content_[3] | 0x80'00'00'00'00'00'00'00 ) };
  }

  static ObjectReference unref_object_reference( ObjectReference ref )
  {
    return __m256i {
      ref.content_[0], ref.content_[1], ref.content_[2], ref.content_[3] & 0x7f'ff'ff'ff'ff'ff'ff'ff
    };
  }
};
