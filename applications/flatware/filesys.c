#include "filesys.h"
#include "asm-flatware.h"
#include "flatware-decs.h"
#include <stdlib.h>
#include <string.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"

/**
 * @brief Checks if a file is a directory
 *
 * @param dirent_ROTable_index index of the ro_table to check
 * @return bool true if the file is a directory, false otherwise
 */
bool is_dir( int32_t dirent_ROTable_index )
{
  externref ret = get_content( dirent_ROTable_index );

  if ( fixpoint_is_blob( ret ) == 1 ) { // if is blob
    return false;
  }

  return true;
}

/**
 * @brief Copies the contents of a ro_table to a flatware memory buffer
 *
 * @param dirent_ROTable_index index of the ro_table to copy
 * @return struct substring the name of the file
 */
struct substring get_name( int32_t dirent_ROTable_index )
{
  int blob_size;
  char* buf;
  struct substring name;

  externref ret = get_ro_table( dirent_ROTable_index, 0 );

  attach_blob_ro_mem( ScratchROMem, ret );

  blob_size = byte_size_ro_mem( ScratchROMem );

  buf = (char*)malloc( (unsigned long)blob_size );

  ro_mem_to_flatware_mem( ScratchROMem, (int32_t)buf, 0, blob_size );

  name.ptr = (char*)buf;
  name.len = (size_t)blob_size;
  return name;
}

/**
 * @brief Gets the permissions of a file
 *
 * @param dirent_ROTable_index index of the ro_table to check
 * @return uint64_t the permissions of the file
 */
uint64_t get_permissions( int32_t dirent_ROTable_index )
{
  int32_t blob_size;
  externref ret;
  uint64_t buf = 0;

  ret = get_ro_table( dirent_ROTable_index, DIRENT_PERMISSIONS );

  attach_blob_ro_mem( ScratchROMem, ret );

  blob_size = byte_size_ro_mem( ScratchROMem );

  ro_mem_to_flatware_mem( ScratchROMem, (int32_t)&buf, 0, blob_size );

  return buf;
}

/**
 * @brief Get the content from a dirent ro_table
 *
 * @param dirent_ROTable_index index of the ro_table to check
 * @return externref the content of the ro_table
 */
externref get_content( int32_t dirent_ROTable_index )
{
  return get_ro_table( dirent_ROTable_index, DIRENT_CONTENT );
}

/**
 * @brief Finds a dirent in a directory (one-level deep)
 *
 * @param path path of the dirent to find
 * @param dir_ROTable_index index of the ro_table containing the directory
 * @param dest_ROTable_index index of the ro_table to store the dirent
 * @return int32_t index of the ro_table containing the dirent if found, FILESYS_SEARCH_FAILED otherwise
 */
int32_t find_local_entry( struct substring path, int32_t dir_ROTable_index, int32_t dest_ROTable_index )
{
  int32_t num_dirents;
  struct substring name;

  if ( path.len == 0 ) {
    return dir_ROTable_index;
  }
  if ( !is_dir( dir_ROTable_index ) ) {
    return FILESYS_SEARCH_FAILED;
  }
  
  attach_tree_ro_table( ScratchFileROTable, get_content( dir_ROTable_index ) );
  num_dirents = size_ro_table( ScratchFileROTable );

  for ( int i = 0; i < num_dirents; i++ ) {
    attach_tree_ro_table( dest_ROTable_index, get_ro_table( ScratchFileROTable, i ) );
    name = get_name( dest_ROTable_index );

    if ( name.len == path.len && memcmp( name.ptr, path.ptr, path.len ) == 0 ) {
      return dest_ROTable_index;
    }
  }

  return FILESYS_SEARCH_FAILED;
}

/**
 * @brief Finds a dirent in a directory (deep search)
 *
 * @param path slash-separated path of the dirent to find
 * @param dir_ROTable_index index of the ro_table containing the directory
 * @param dest_ROTable_index index of the ro_table to store the dirent
 * @return int32_t index of the ro_table containing the dirent if found, FILESYS_SEARCH_FAILED otherwise
 */
int32_t find_deep_entry( struct substring path, int32_t dir_ROTable_index, int32_t dest_ROTable_index )
{
  void* slash_ptr;
  while ( ( slash_ptr = memchr( path.ptr, '/', path.len ) ) ) {
    const size_t component_len = (size_t)( (char*)slash_ptr - (char*)path.ptr );
    const struct substring component = { path.ptr, component_len };
    path = ( struct substring ) { (char*)slash_ptr + 1, path.len - component_len - 1 };

    dir_ROTable_index = find_local_entry( component, dir_ROTable_index, dest_ROTable_index );
    if ( dir_ROTable_index < 0 ) {
      return FILESYS_SEARCH_FAILED;
    }
  }
  return find_local_entry( path, dir_ROTable_index, dest_ROTable_index );
}

#pragma clang diagnostic pop
