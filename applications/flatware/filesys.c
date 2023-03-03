#include "filesys.h"
#include "asm-flatware.h"
#include <stdlib.h>
#include <string.h>

bool is_dir( int32_t ro_table_index )
{
  externref ret = get_ro_content( ro_table_index );

  if ( value_type( ret ) == 0 ) { // if is tree
    return true;
  }

  return false;
}

struct substring get_name( int32_t ro_table_index )
{
  int blob_size;
  char* buf;
  struct substring name;

  externref ret = get_ro_table( ro_table_index, 0 );

  attach_blob_ro_mem( FileSystemDataROMem, ret );

  blob_size = byte_size_ro_mem( FileSystemDataROMem );

  buf = (char*)malloc( (unsigned long)blob_size );

  ro_mem_to_flatware_mem( FileSystemDataROMem, buf, 0, blob_size );

  name.ptr = (char*)buf;
  name.len = (size_t)blob_size;
  return name;
}

uint64_t get_permissions( int32_t ro_table_index )
{
  int32_t blob_size;
  externref ret;
  uint64_t buf = 0;

  ret = get_ro_table( ro_table_index, 1 );

  attach_blob_ro_mem( FileSystemDataROMem, ret );

  blob_size = byte_size_ro_mem(FileSystemDataROMem);

  ro_mem_to_flatware_mem(FileSystemDataROMem, (char*)&buf, 0, blob_size );

  return buf;
}

externref get_ro_content( int32_t ro_table_index )
{
  return get_ro_table( ro_table_index, 2 );
}

externref get_rw_content( int32_t rw_table_index )
{
  return get_rw_table( rw_table_index, 2 );
}

int32_t find_local_file( struct substring path, int32_t curr_fd, bool must_be_dir, int32_t desired_fd, bool wd_dirty )
{
  externref dirent_content;
  int32_t num_of_dirents;

  if (!wd_dirty) {
    dirent_content = get_ro_content( curr_fd );
    attach_tree_ro_table( ScratchROTable0, dirent_content );
    num_of_dirents = size_ro_table( ScratchROTable0 );
  } else {
    //TODO: figure out how to read new dirty home directory
    //dirent_content = get_rw_content( curr_fd );
    num_of_dirents = 0;
  }

  for ( int i = 0; i < num_of_dirents; i++ ) {
    struct substring name;
    externref subdirent = get_ro_table( ScratchROTable0, i );
    attach_tree_ro_table( desired_fd, subdirent );
    name = get_name( desired_fd );
    
    if ( memcmp( name.ptr, path.ptr, path.len ) == 0 && name.len == path.len ) {
      attach_tree_ro_table( desired_fd, subdirent );
      if ( !is_dir( desired_fd ) && must_be_dir ) {
        return -1;
      } else if ( is_dir( desired_fd ) && !must_be_dir ) { // hmmmmm, not necessary? can open file or dir
        return -1;
      }
      return desired_fd;
    }
  }

  return ( 0 - (int32_t) path.len );
}

int32_t lookup( substring path, int32_t curr_fd, int32_t desired_fd, bool is_dir, bool wd_dirty )
{
  while ( 1 ) {
    void* slash_ptr = memchr( path.ptr, '/', path.len );
    if ( slash_ptr ) {
      const size_t component_len = (size_t)( (char*) slash_ptr - (char*) path.ptr );
      const substring curr_component = { path.ptr, component_len };
      const substring rest_of_path = { (char*) slash_ptr + 1, path.len - component_len - 1 };

      curr_fd = find_local_file( curr_component, curr_fd, true, desired_fd, wd_dirty );
      if ( curr_fd < 0 ) {
        // return curr_fd;
        return ( 0 - (int32_t)( component_len ) );
      }
      path = rest_of_path;
      continue;
    } else {
      return find_local_file( path, curr_fd, is_dir, desired_fd, wd_dirty );
    }
  }
}

int32_t find_rw_index( substring child, int32_t curr_fd, int32_t temp_fd, bool wd_dirty ) {
  externref dirent_content;
  int32_t num_of_dirents;

  child = find_child_file_name( child );
  if (!wd_dirty) {
    dirent_content = get_ro_content( curr_fd );
    attach_tree_ro_table( ScratchROTable0, dirent_content );
    num_of_dirents = size_ro_table( ScratchROTable0 );
  } else {
    //TODO: figure out how to read new dirty home directory
    //dirent_content = get_rw_content( curr_fd );
    //num_of_dirents = size_rw_table( curr_fd );
    num_of_dirents = 0;
  }

   for ( int i = 0; i < num_of_dirents; i++ ) {
    struct substring name;
    externref subdirent = get_ro_table( ScratchROTable0, i );
    attach_tree_ro_table( temp_fd, subdirent );
    name = get_name( temp_fd );
    if ( memcmp( name.ptr, child.ptr, child.len ) == 0 && name.len == child.len ) {
      return i;
    }
  }
  return -1;
}

// needs error checking!
int32_t find_file( int32_t path, // offset into main memory of sloth program, needs to be copied to flatware memory
                   int32_t path_len,
                   int32_t curr_fd,
                   int32_t desired_fd,
                   bool wd_dirty)
{
  char* buf;
  substring my_path;
  int32_t result;

  buf = (char*) malloc( (unsigned long) path_len );

  program_memory_to_flatware_memory( buf, path, path_len );

  my_path = ( substring ) { buf, (size_t) path_len };

  result = lookup( my_path, curr_fd, desired_fd, false, wd_dirty );

  free( buf );

  return result;
}

substring find_file_path( int32_t path, int32_t path_len) {
  char* buf;
  substring my_path;

  buf = (char*) malloc( (unsigned long) path_len );
  program_memory_to_flatware_memory( buf, path, path_len );
  my_path = ( substring ) { buf, (size_t) path_len };
  return my_path;
}

substring find_parent_file_path( substring path ) {
  char* ptr = (char *) (path.ptr);
  for (size_t i = path.len - 1; i != 0; i--) {
    if (ptr[i] == '/') {
      return (substring) { (char*) path.ptr, i };
    }
  }  
  return (substring) { (char*) path.ptr, 0 };
}

substring find_child_file_name( substring path ) {
  char* ptr = (char *) (path.ptr);
  for (size_t i = path.len - 1; i != 0; i--) {
    if (ptr[i] == '/') {
      return (substring) { (char*) path.ptr + i + 1, path.len - i - 1 };
    }
  }  
  return (substring) { (char*) path.ptr, 0 };
}
