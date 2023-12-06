#pragma once
#include <concepts>
#include <string_view>
#include <vector>

#include "mmap.hh"
#include "runtime.hh"

#define COMPILE_ENCODE *Runtime::get_instance().storage().get_ref( "compile-encode" )

struct deep_pretty_print
{
  Handle name;
  unsigned int level;
  deep_pretty_print( Handle name, unsigned int level = 0 )
    : name( name )
    , level( level )
  {}
};

struct pretty_print
{
  Handle name;
  pretty_print( Handle name )
    : name( name )
  {}
};

struct pretty_print_relation
{
  Relation relation;
  pretty_print_relation( Relation relation )
    : relation( relation )
  {}
};

struct pretty_print_handle
{
  Handle handle;
  pretty_print_handle( Handle handle )
    : handle( handle )
  {}
};

std::ostream& operator<<( std::ostream& stream, const deep_pretty_print& pp );
std::ostream& operator<<( std::ostream& stream, const pretty_print& pp );
std::ostream& operator<<( std::ostream& stream, const pretty_print_relation& pp );
std::ostream& operator<<( std::ostream& stream, const pretty_print_handle& pp );

Handle parse_args( span_view<char*>& args, std::vector<ReadOnlyFile>& open_files, bool deserialize = false );
