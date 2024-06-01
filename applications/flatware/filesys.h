#include "api.h"
#include "util/hashmap.h"
#include "util/linked_list.h"
#include "util/string_view.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

enum DIRENT
{
  DIRENT_NAME = 0,
  DIRENT_CONTENT = 1,
  DIRENT_METADATA = 2,
};

struct DIRENT_METADATA
{
  uint64_t link_count;
  uint64_t access_timestamp;
  uint64_t modify_timestamp;
  uint64_t change_timestamp;
};

typedef struct entry entry;

struct entry
{
  char* name;
  entry* parent;

  __wasi_filestat_t stat;
  bool dirty;
  bool accessed;

  char* content;
  struct list children;

  struct list_elem elem;
};

typedef struct filedesc
{
  int32_t index;
  uint64_t offset;
  entry* entry;

  __wasi_fdstat_t stat;

  struct hash_elem elem;
} filedesc;

typedef struct fd_table
{
  struct hash fds;
  uint32_t next_fd;
} fd_table;

entry* entry_load_filesys( entry* parent, externref node );
externref entry_save_filesys( entry* e );
entry* entry_from_blob( const char* name, externref content );
externref entry_to_blob( entry* e );
entry* entry_create( const char* name, __wasi_filetype_t type );
void entry_free( entry* e );
bool entry_grow( entry* e, uint64_t size );
entry* entry_find( entry* e, const char* name );

void fd_table_init( fd_table* f );
void fd_table_destroy( fd_table* f );
filedesc* fd_table_insert( fd_table* f, entry* e );
filedesc* fd_table_insert_at( fd_table* f, entry* e, int32_t index );
filedesc* fd_table_find( fd_table* f, int32_t index );
bool fd_table_remove( fd_table* f, int32_t index );
