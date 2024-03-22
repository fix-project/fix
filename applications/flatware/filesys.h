#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum DIRENT
{
  DIRENT_NAME = 0,
  DIRENT_PERMISSIONS = 1,
  DIRENT_CONTENT = 2,
};

struct substring
{
  void* ptr;
  size_t len;
};

static const int8_t FILESYS_SEARCH_FAILED = -1;

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

bool is_dir( int32_t dirent_ROTable_index );

struct substring get_name( int32_t dirent_ROTable_index );

uint64_t get_permissions( int32_t dirent_ROTable_index );

externref get_content( int32_t dirent_ROTable_index );

int32_t find_local_entry( struct substring path, int32_t dir_ROTable_index, int32_t dest_ROTable_index );

int32_t find_deep_entry( struct substring path, int32_t curr_fd, int32_t desired_fd );
