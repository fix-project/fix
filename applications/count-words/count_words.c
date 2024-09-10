#pragma clang diagnostic ignored "-Wignored-attributes"

#include "fixpoint_util.h"
#include "support.h"

#include <stdlib.h>
#include <string.h>

void out( const char* s )
{
  fixpoint_unsafe_io( s, strlen( s ) );
}

__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref combination )
{
  attach_tree_ro_table_0( combination );
  size_t argc = get_length( combination );
  if (argc != 3) {
    out("incorrect argument to count_words, expected tuple of: (pattern, file)\n");
    return create_blob_i64(-1);
  }

  /* auto rlimits = get_ro_table_0( 0 ); */
  /* auto self = get_ro_table_0( 1 ); */
  externref tuple = get_ro_table_0(2);
  argc = get_length( tuple );
  if (argc != 2) {
    out("incorrect argument to count_words, expected tuple of: (pattern, file)\n");
    return create_blob_i64(-1);
  }
  attach_tree_ro_table_0(tuple);
  externref pattern = get_ro_table_0( 0 );
  externref file = get_ro_table_0( 1 );

  size_t needle_size = get_length( pattern );
  char *needle = malloc(needle_size);
  attach_blob_ro_mem_0(pattern);
  ro_mem_0_to_program_mem( needle, 0, needle_size );

  size_t haystack_size = get_length( file );
  attach_blob_ro_mem_0(file);
  char* haystack = (char*)malloc( haystack_size );
  ro_mem_0_to_program_mem( haystack, 0, haystack_size );

  size_t count = 0;
  if (needle_size <= haystack_size) {
    for (size_t i = 0; i < haystack_size - needle_size + 1; i++) {
      if (!memcmp(needle, haystack + i, needle_size)) {
        count++;
        i += needle_size - 1;
      }
    }
  }

  return create_blob_i64(count);
}
