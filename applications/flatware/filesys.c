#include "filesys.h"
#include "flatware-decs.h"
#include "util/hashmap.h"
#include "util/linked_list.h"
#include "util/macros.h"
#include <stdlib.h>
#include <string.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"

static uint64_t rw_table_index = 0;

static void entry_load_file( entry* e, externref contents );
static void entry_load_dir( entry* e, externref contents );
static externref entry_save_file( entry* e );
static externref entry_save_dir( entry* e );
static filedesc* filedesc_create( entry* e );
static void filedesc_free( filedesc* f );
static hash_hash_func filedesc_hash;
static hash_less_func filedesc_less;
static hash_action_func filedesc_destroy;

entry* entry_load_filesys( entry* parent, externref node )
{
  attach_tree_ro_table( ScratchROTable, node );
  if ( size_ro_table( ScratchROTable ) < 3 ) {
    return NULL;
  }

  entry* e = (entry*)malloc( sizeof( entry ) );
  if ( e == NULL ) {
    return NULL;
  }

  e->parent = parent;

  attach_blob_ro_mem( ScratchROMem, get_ro_table( ScratchROTable, 0 ) );
  e->name = malloc( (unsigned long)byte_size_ro_mem( ScratchROMem ) + 1 );
  if ( e->name == NULL ) {
    free( e );
    return NULL;
  }
  ro_mem_to_flatware_mem( ScratchROMem, (int32_t)e->name, 0, byte_size_ro_mem( ScratchROMem ) );
  e->name[byte_size_ro_mem( ScratchROMem )] = '\0';

  e->dirty = false;
  e->accessed = false;
  list_init( &e->children );

  e->stat.dev = 0;
  e->stat.ino = 0;

  externref contents = get_ro_table( ScratchROTable, 2 );
  if ( fixpoint_is_blob( contents ) ) {
    e->stat.filetype = __WASI_FILETYPE_REGULAR_FILE;
    entry_load_file( e, contents );
  } else {
    e->stat.filetype = __WASI_FILETYPE_DIRECTORY;
    e->content = NULL;
    entry_load_dir( e, contents );
  }
  return e;
}

static void entry_load_file( entry* e, externref contents )
{
  attach_blob_ro_mem( ScratchROMem, contents );
  e->stat.size = byte_size_ro_mem( ScratchROMem );
  e->content = malloc( (unsigned long)e->stat.size );
  if ( e->content == NULL ) {
    return;
  }
  ro_mem_to_flatware_mem( ScratchROMem, (int32_t)e->content, 0, e->stat.size );
}

static void entry_load_dir( entry* e, externref contents )
{
  attach_tree_ro_table( ScratchROTable, contents );
  e->stat.size = size_ro_table( ScratchROTable );

  for ( uint64_t i = 0; i < e->stat.size; i++ ) {
    attach_tree_ro_table( ScratchROTable, contents );
    entry* child = entry_load_filesys( e, get_ro_table( ScratchROTable, i ) );
    if ( child == NULL ) {
      return;
    }
    list_push_back( &e->children, &child->elem );
  }
}

externref entry_save_filesys( entry* e )
{
  int32_t name_len = (int32_t)strlen( e->name );
  if ( name_len > page_size_rw_mem( ScratchRWMem ) * WASM_RT_PAGESIZE ) {
    grow_rw_mem_pages( ScratchRWMem,
                       ( name_len - page_size_rw_mem( ScratchRWMem ) * WASM_RT_PAGESIZE ) / WASM_RT_PAGESIZE + 1 );
  }
  flatware_mem_to_rw_mem( ScratchRWMem, 0, (int32_t)e->name, name_len );
  externref name = create_blob_rw_mem( ScratchRWMem, name_len );
  externref contents;
  if ( e->stat.filetype == __WASI_FILETYPE_REGULAR_FILE ) {
    contents = entry_save_file( e );
  } else {
    contents = entry_save_dir( e );
  }
  if ( size_rw_table( ScratchRWTable ) < 2 ) {
    grow_rw_table( ScratchRWTable, 2, create_blob_i32( 0 ) );
  }
  set_rw_table( ScratchRWTable, 0, name );
  set_rw_table( ScratchRWTable, 1, contents );
  return create_tree_rw_table( ScratchRWTable, 2 );
}

static externref entry_save_file( entry* e )
{
  if ( e->stat.size > (uint32_t)page_size_rw_mem( ScratchRWMem ) * WASM_RT_PAGESIZE ) {
    grow_rw_mem_pages(
      ScratchRWMem, ( e->stat.size - page_size_rw_mem( ScratchRWMem ) * WASM_RT_PAGESIZE ) / WASM_RT_PAGESIZE + 1 );
  }
  flatware_mem_to_rw_mem( ScratchRWMem, 0, (int32_t)e->content, e->stat.size );
  return create_blob_rw_mem( ScratchRWMem, e->stat.size );
}

static externref entry_save_dir( entry* e )
{
  if ( e->stat.size + rw_table_index > (uint32_t)size_rw_table( FileSystemRWTable ) ) {
    grow_rw_table( FileSystemRWTable, e->stat.size, create_blob_i32( 0 ) );
  }
  uint64_t initial_index = rw_table_index;
  rw_table_index += e->stat.size;

  uint64_t i = initial_index;
  for ( struct list_elem* elem = list_begin( &e->children ); elem != list_end( &e->children );
        elem = list_next( elem ) ) {
    set_rw_table( FileSystemRWTable, i++, entry_save_filesys( list_entry( elem, entry, elem ) ) );
  }

  if ( e->stat.size > (uint32_t)size_rw_table( ScratchRWTable ) ) {
    grow_rw_table( ScratchRWTable, e->stat.size, create_blob_i32( 0 ) );
  }
  for ( uint64_t i = 0; i < e->stat.size; i++ ) {
    set_rw_table( ScratchRWTable, i, get_rw_table( FileSystemRWTable, initial_index + i ) );
  }

  rw_table_index = initial_index;
  return create_tree_rw_table( ScratchRWTable, e->stat.size );
}

void entry_free( entry* e )
{
  if ( e->stat.filetype == __WASI_FILETYPE_DIRECTORY ) {
    for ( struct list_elem* elem = list_begin( &e->children ); elem != list_end( &e->children );
          elem = list_next( elem ) ) {
      entry_free( list_entry( elem, entry, elem ) );
    }
  } else {
    free( e->content );
  }
  free( e->name );
  free( e );
}

entry* entry_from_blob( const char* name, externref content )
{
  entry* e = (entry*)malloc( sizeof( entry ) );
  if ( e == NULL ) {
    return NULL;
  }

  e->parent = NULL;
  e->name = strdup( name );
  e->dirty = false;
  e->accessed = false;
  list_init( &e->children );

  e->stat.filetype = __WASI_FILETYPE_REGULAR_FILE;
  attach_blob_ro_mem( ScratchROMem, content );
  e->stat.size = byte_size_ro_mem( ScratchROMem );
  e->content = malloc( (unsigned long)e->stat.size );
  if ( e->content == NULL ) {
    return NULL;
  }
  ro_mem_to_flatware_mem( ScratchROMem, (int32_t)e->content, 0, e->stat.size );

  return e;
}

externref entry_to_blob( entry* e )
{
  if ( e->stat.size > (uint32_t)page_size_rw_mem( ScratchRWMem ) * WASM_RT_PAGESIZE ) {
    grow_rw_mem_pages(
      ScratchRWMem, ( e->stat.size - page_size_rw_mem( ScratchRWMem ) * WASM_RT_PAGESIZE ) / WASM_RT_PAGESIZE + 1 );
  }
  flatware_mem_to_rw_mem( ScratchRWMem, 0, (int32_t)e->content, e->stat.size );
  return create_blob_rw_mem( ScratchRWMem, e->stat.size );
}

entry* entry_create( const char* name, __wasi_filetype_t type )
{
  entry* e = (entry*)malloc( sizeof( entry ) );
  if ( e == NULL ) {
    return NULL;
  }

  e->parent = NULL;
  e->name = strdup( name );
  e->dirty = false;
  e->accessed = false;
  list_init( &e->children );

  e->stat.size = 0;
  e->content = NULL;

  e->stat.dev = 0;
  e->stat.ino = 0;
  e->stat.filetype = type;

  return e;
}

static filedesc* filedesc_create( entry* e )
{
  filedesc* fd = (filedesc*)malloc( sizeof( filedesc ) );
  if ( fd == NULL ) {
    return NULL;
  }

  fd->index = -1;
  fd->offset = 0;
  fd->entry = e;

  fd->stat.fs_filetype = e->stat.filetype;
  fd->stat.fs_rights_base = ~0;
  fd->stat.fs_rights_inheriting = ~0;
  fd->stat.fs_flags = 0;

  return fd;
}

static void filedesc_free( filedesc* fd )
{
  free( fd );
}

void fd_table_init( fd_table* f )
{
  hash_init( &f->fds, filedesc_hash, filedesc_less, NULL );
  f->next_fd = 0;
}

void fd_table_destroy( fd_table* f )
{
  hash_destroy( &f->fds, filedesc_destroy );
}

filedesc* fd_table_insert( fd_table* f, entry* e )
{
  filedesc* fd = filedesc_create( e );
  if ( fd == NULL ) {
    return NULL;
  }

  while ( fd_table_find( f, f->next_fd ) != NULL ) {
    f->next_fd++;
  }
  fd->index = f->next_fd++;

  hash_insert( &f->fds, &fd->elem );
  return fd;
}

filedesc* fd_table_insert_at( fd_table* f, entry* e, int32_t index )
{
  filedesc* fd = filedesc_create( e );
  if ( fd == NULL ) {
    return NULL;
  }

  fd->index = index;
  hash_insert( &f->fds, &fd->elem );

  return fd;
}

filedesc* fd_table_find( fd_table* f, int32_t index )
{
  filedesc fd;
  fd.index = index;
  struct hash_elem* e = hash_find( &f->fds, &fd.elem );
  if ( e == NULL ) {
    return NULL;
  }

  return hash_entry( e, filedesc, elem );
}

bool fd_table_remove( fd_table* f, int32_t index )
{
  filedesc fd;
  fd.index = index;
  struct hash_elem* e = hash_delete( &f->fds, &fd.elem );
  if ( e == NULL ) {
    return false;
  }
  filedesc_free( hash_entry( e, filedesc, elem ) );
  return true;
}

static unsigned filedesc_hash( const struct hash_elem* e, void* aux UNUSED )
{
  filedesc* fd = hash_entry( e, filedesc, elem );
  return hash_int( fd->index );
}

static bool filedesc_less( const struct hash_elem* a, const struct hash_elem* b, void* aux UNUSED )
{
  filedesc* fd_a = hash_entry( a, filedesc, elem );
  filedesc* fd_b = hash_entry( b, filedesc, elem );
  return fd_a->index < fd_b->index;
}

static void filedesc_destroy( struct hash_elem* e, void* aux UNUSED )
{
  filedesc* fd = hash_entry( e, filedesc, elem );
  filedesc_free( fd );
}

bool entry_grow( entry* e, uint64_t size )
{
  if ( e->stat.filetype != __WASI_FILETYPE_BLOCK_DEVICE && e->stat.filetype != __WASI_FILETYPE_CHARACTER_DEVICE
       && e->stat.filetype != __WASI_FILETYPE_REGULAR_FILE ) {
    return false;
  }

  if ( size <= e->stat.size ) {
    return true;
  }

  e->content = realloc( e->content, (unsigned long)size );
  if ( e->content == NULL ) {
    return false;
  }
  memset( e->content + e->stat.size, 0, (unsigned long)( size - e->stat.size ) );

  e->stat.size = size;
  return true;
}

entry* entry_find( entry* e, const char* name )
{
  if ( e->stat.filetype != __WASI_FILETYPE_DIRECTORY ) {
    return NULL;
  }

  for ( struct list_elem* elem = list_begin( &e->children ); elem != list_end( &e->children );
        elem = list_next( elem ) ) {
    entry* child = list_entry( elem, entry, elem );
    if ( strcmp( child->name, name ) == 0 ) {
      return child;
    }
  }

  return NULL;
}

#pragma clang diagnostic pop
