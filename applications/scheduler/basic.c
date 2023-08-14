#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/fixpoint_debug.h"

#define FIXPOINT_APPLY __attribute__( ( export_name( "_fixpoint_apply" ) ) )

int println( const char* fmt, ... )
{
  va_list args;

  int ret;
  va_start( args, fmt );
  size_t n = 1024;
  while ( 1 ) {
    va_list copy;
    char* s = malloc( n );
    va_copy( copy, args );
    ret = vsnprintf( s, n, fmt, copy );
    va_end( copy );
    if ( ret >= 0 && ret < n ) {
      fixpoint_unsafe_io( s, ret );
      free( s );
      break;
    } else {
      free( s );
      n *= 2;
    }
  }
  va_end( args );
  return ret;
}

const char* op_names[] = {
  "Apply",
  "Evaluate",
  "Fill",
};

const char* type_names[] = {
  "Tree",
  "Thunk",
  "Blob",
  "Tag",
};

const char* access_names[] = {
  "Strict",
  "Shallow",
  "Lazy",
};

FIXPOINT_APPLY externref _fixpoint_apply( externref encode )
{
  println( "scheduling:" );
  attach_tree_ro_table_0( encode );
  externref task = get_ro_table_0( 2 );

  attach_tree_ro_table_1( task );
  externref operation = get_ro_table_1( 0 );
  externref handle = get_ro_table_1( 1 );
  externref accessibility = get_ro_table_1( 2 );

  attach_blob_ro_mem_0( operation );
  attach_blob_ro_mem_1( accessibility );

  uint32_t op = get_i32_ro_mem_0( 0 );
  uint32_t type = get_value_type( handle );
  uint32_t access = get_i32_ro_mem_1( 0 );

  println( "operation: %d (%s)", op, op_names[op] );
  println( "type: %d (%s)", type, type_names[type] );
  println( "accessibility: %d (%s)", access, access_names[access] );
  externref lifted = debug_try_lift( handle );
  access = get_access( lifted );
  println( "lifted: %d (%s)", access, access_names[access] );
  println( "" );
  return create_blob_i32( 0 );
}
