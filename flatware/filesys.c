#include "filesys.h"
#include "asm-flatware.h"
#include <stdlib.h>
#include <string.h>

bool is_dir( int32_t ro_table_index )
{
  externref ret = get_content( ro_table_index );

  if ( value_type( ret ) == 1 ) { // if is tree
    return true;
  }

  return false;
}

const char* get_name( int32_t ro_table_index )
{
  int blob_size;
  char* buf;

  externref ret = get_ro_table( ro_table_index, 0 );

  attach_blob_ro_mem_2( ret );

  blob_size = size_ro_mem_2();

  buf = (char*)malloc( (unsigned long)blob_size );

  ro_2_to_flatware_memory( buf, 0, blob_size );

  return buf;
}

uint64_t get_permissions( int32_t ro_table_index )
{
  int32_t blob_size;
  externref ret;
  uint64_t buf = 0;

  ret = get_ro_table( ro_table_index, 1 );

  attach_blob_ro_mem_2( ret );

  blob_size = size_ro_mem_2();

  ro_2_to_flatware_memory( &buf, 0, blob_size );

  return buf;
}

externref get_content( int32_t ro_table_index )
{
  return get_ro_table( ro_table_index, 2 );
}

int32_t find_local_file( struct substring path, int32_t curr_fd, bool should_be_dir, int32_t desired_fd )
{
  externref dirent_content;
  int32_t num_of_dirents;

  dirent_content = get_content( curr_fd );
  attach_tree_ro_table_2( dirent_content );
  num_of_dirents = size_ro_table_2();

  for ( int i = 0; i < num_of_dirents; i++ ) {
    const char* name;
    externref subdirent = get_ro_table( 2, i );
    attach_tree_ro_table( desired_fd, subdirent );
    name = get_name( desired_fd );

    if ( strcmp( name, path.ptr ) == 0 ) {
      attach_tree_ro_table( desired_fd, subdirent );
      if ( !is_dir( desired_fd ) && should_be_dir ) {
        return -1;
      } else if ( is_dir( desired_fd ) && !should_be_dir ) {
        return -1;
      }
      return desired_fd;
    }
  }

  return -1; // not found
}

int32_t lookup( struct substring path, int32_t curr_fd, int32_t desired_fd )
{
  while ( 1 ) {
    void* slash_ptr = memchr( path.ptr, '/', path.len );
    if ( slash_ptr ) {
      const size_t component_len = (size_t)( (char*)slash_ptr - (char*)path.ptr );

      const struct substring curr_component = { path.ptr, component_len };
      const struct substring rest_of_path = { (char*)slash_ptr + 1, path.len - component_len - 1 };

      curr_fd = find_local_file( curr_component, curr_fd, true, desired_fd );
      if ( curr_fd < 0 ) {
        return -1;
      }
      path = rest_of_path;
      continue;
    } else {
      return find_local_file( path, curr_fd, false, desired_fd );
    }
  }
}

// needs error checking!
int32_t find_file( int32_t path, // offset into main memory of sloth program, needs to be copied to flatware memory
                   int32_t path_len,
                   int32_t curr_fd,
                   int32_t desired_fd )
{
  char* buf;
  struct substring my_path;

  buf = (char*)malloc( (unsigned long)path_len );

  program_memory_to_flatware_memory( buf, path, path_len );

  my_path = ( struct substring ) { buf, strlen( buf ) };

  return lookup( my_path, curr_fd, desired_fd );
}
