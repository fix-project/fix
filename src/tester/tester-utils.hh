#pragma once
#include <concepts>
#include <string_view>
#include <vector>

#include "mmap.hh"
#include "runtime.hh"

#define COMPILE_ENCODE *Runtime::get_instance().storage().get_ref( "compile-encode" )

struct pretty_print
{
  Handle name;
  unsigned int level;
  pretty_print( Handle name, unsigned int level = 0 )
    : name( name )
    , level( level )
  {}
};

std::ostream& operator<<( std::ostream& stream, const pretty_print& pp );

Handle parse_args( span_view<char*>& args, std::vector<ReadOnlyFile>& open_files );
