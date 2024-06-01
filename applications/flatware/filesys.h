#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "util/linked_list.h"
#include "util/hashmap.h"
#include "util/string_view.h"

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

enum ENTRY_TYPE
{
  ENTRY_FILE,
  ENTRY_DIR,
};

struct entry
{
  enum ENTRY_TYPE type;
  entry* parent;

  char* name;
  int64_t size;
  struct DIRENT_METADATA metadata;
  bool dirty;
  bool accessed;

  char* content;
  struct list children;

  struct list_elem elem;
};

typedef struct filedesc
{
  int32_t index;
  int64_t offset;
  entry* entry;

  struct hash_elem elem;
} filedesc;

typedef struct fd_table {
  struct hash fds;
  uint32_t next_fd;
} fd_table;

entry* entry_load_filesys( entry* parent, externref node );
externref entry_save_filesys( entry* e );
entry* entry_from_blob( const char* name, externref content );
externref entry_to_blob( entry* e ); 
entry* entry_create( const char* name, enum ENTRY_TYPE type );
void entry_free( entry* e );
bool entry_grow( entry* e, uint64_t size );
entry* entry_find( entry* e, const char* name );

void fd_table_init( fd_table* f );
void fd_table_destroy( fd_table* f );
int32_t fd_table_insert( fd_table* f, entry* e );
bool fd_table_insert_at( fd_table* f, entry* e, int32_t index );
filedesc* fd_table_find( fd_table* f, int32_t index );
bool fd_table_remove( fd_table* f, int32_t index );
