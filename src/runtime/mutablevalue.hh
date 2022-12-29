#pragma once

#include "cookie.hh"
#include "name.hh"

/**
 * TreeEntry metadata:
 * if the tree is a MTree before freezing: | assessible/not accessible | other/literal | strict/lazy(intended) | the
 * same as underlying name otherwise, the same as Name
 */
class MTreeEntry : public cookie_name
{
public:
  MTreeEntry() = default;

  operator __m256i() const { return content_; }

  MTreeEntry( const __m256i val )
    : cookie_name( val )
  {}

  bool is_accessible() const { return !( metadata() & 0x80 ); }

  bool is_intended_strict() const { return !( metadata() & 0x20 ); }

  static __m256i to_name( MTreeEntry entry )
  {
    if ( entry.is_intended_strict() ) {
      return __m256i {
        entry.content_[0], entry.content_[1], entry.content_[2], entry.content_[3] & 0x5f'ff'ff'ff'ff'ff'ff'ff };
    } else {
      return __m256i {
        entry.content_[0],
        entry.content_[1],
        entry.content_[2],
        static_cast<int64_t>( ( entry.content_[3] & 0x5f'ff'ff'ff'ff'ff'ff'ff ) | 0x80'00'00'00'00'00'00'00 ) };
    }
  }
};
