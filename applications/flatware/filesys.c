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

  externref contents = get_ro_table( ScratchROTable, 2 );
  if ( fixpoint_is_blob( contents ) ) {
    e->type = ENTRY_FILE;
    entry_load_file( e, contents );
  } else {
    e->type = ENTRY_DIR;
    e->content = NULL;
    entry_load_dir( e, contents );
  }
  return e;
}

static void entry_load_file( entry* e, externref contents )
{
  attach_blob_ro_mem( ScratchROMem, contents );
  e->size = byte_size_ro_mem( ScratchROMem );
  e->content = malloc( (unsigned long)e->size );
  if ( e->content == NULL ) {
    return;
  }
  ro_mem_to_flatware_mem( ScratchROMem, (int32_t)e->content, 0, e->size );
}

static void entry_load_dir( entry* e, externref contents )
{
  attach_tree_ro_table( ScratchROTable, contents );
  e->size = size_ro_table( ScratchROTable );

  for ( int64_t i = 0; i < e->size; i++ ) {
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
  if ( e->type == ENTRY_FILE ) {
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
  if ( e->size > (uint32_t)page_size_rw_mem( ScratchRWMem ) * WASM_RT_PAGESIZE ) {
    grow_rw_mem_pages( ScratchRWMem,
                       ( e->size - page_size_rw_mem( ScratchRWMem ) * WASM_RT_PAGESIZE ) / WASM_RT_PAGESIZE + 1 );
  }
  flatware_mem_to_rw_mem( ScratchRWMem, 0, (int32_t)e->content, e->size );
  return create_blob_rw_mem( ScratchRWMem, e->size );
}

static externref entry_save_dir( entry* e )
{
  if ( e->size + rw_table_index > (uint32_t)size_rw_table( FileSystemRWTable ) ) {
    grow_rw_table( FileSystemRWTable, e->size, create_blob_i32( 0 ) );
  }
  uint64_t initial_index = rw_table_index;
  rw_table_index += e->size;

  uint64_t i = initial_index;
  for ( struct list_elem* elem = list_begin( &e->children ); elem != list_end( &e->children );
        elem = list_next( elem ) ) {
    set_rw_table( FileSystemRWTable, i++, entry_save_filesys( list_entry( elem, entry, elem ) ) );
  }


  if ( e->size > (uint32_t)size_rw_table( ScratchRWTable ) ) {
    grow_rw_table( ScratchRWTable, e->size, create_blob_i32( 0 ) );
  }
  for ( int64_t i = 0; i < e->size; i++ ) {
    set_rw_table( ScratchRWTable, i, get_rw_table( FileSystemRWTable, initial_index + i ) );
  }
  
  rw_table_index = initial_index;
  return create_tree_rw_table( ScratchRWTable, e->size );
}

void entry_free( entry* e )
{
  if ( e->type == ENTRY_DIR ) {
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

  e->type = ENTRY_FILE;
  attach_blob_ro_mem( ScratchROMem, content );
  e->size = byte_size_ro_mem( ScratchROMem );
  e->content = malloc( (unsigned long)e->size );
  if ( e->content == NULL ) {
    return NULL;
  }
  ro_mem_to_flatware_mem( ScratchROMem, (int32_t)e->content, 0, e->size );

  return e;
}

externref entry_to_blob( entry* e )
{
  if ( e->size > (uint32_t)page_size_rw_mem( ScratchRWMem ) * WASM_RT_PAGESIZE ) {
    grow_rw_mem_pages( ScratchRWMem,
                       ( e->size - page_size_rw_mem( ScratchRWMem ) * WASM_RT_PAGESIZE ) / WASM_RT_PAGESIZE + 1 );
  }
  flatware_mem_to_rw_mem( ScratchRWMem, 0, (int32_t)e->content, e->size );
  return create_blob_rw_mem( ScratchRWMem, e->size );
}

entry* entry_create( const char* name, enum ENTRY_TYPE type )
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

  e->type = type;
  e->size = 0;
  e->content = NULL;

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

int32_t fd_table_insert( fd_table* f, entry* e )
{
  filedesc* fd = filedesc_create( e );
  if ( fd == NULL ) {
    return -1;
  }

  while ( fd_table_find( f, f->next_fd ) != NULL ) {
    f->next_fd++;
  }
  fd->index = f->next_fd++;

  hash_insert( &f->fds, &fd->elem );
  return fd->index;
}

bool fd_table_insert_at( fd_table* f, entry* e, int32_t index )
{
  filedesc* fd = filedesc_create( e );
  if ( fd == NULL ) {
    return false;
  }

  fd->index = index;
  hash_insert( &f->fds, &fd->elem );

  return true;
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
  if ( e->type != ENTRY_FILE ) {
    return false;
  }

  e->content = realloc( e->content, (unsigned long)size );
  if ( e->content == NULL ) {
    return false;
  }
  memset( e->content + e->size, 0, (unsigned long)( size - e->size ) );

  e->size = size;
  return true;
}

entry* entry_find( entry* e, const char* name )
{
  if ( e->type != ENTRY_DIR ) {
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
