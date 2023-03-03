#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct substring
{
  void* ptr;
  size_t len;
} substring;

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

bool is_dir( int32_t ro_table_index );

struct substring get_name( int32_t ro_table_index );

uint64_t get_permissions( int32_t ro_table_index );

externref get_ro_content( int32_t ro_table_index );

externref get_rw_content( int32_t rw_table_index );

int32_t find_local_file( struct substring path, int32_t curr_fd, bool should_be_dir, int32_t desired_fd, bool wd_dirty );

int32_t find_file( int32_t path, int32_t path_len, int32_t curr_fd, int32_t desired_fd, bool wd_dirty );

substring find_file_path( int32_t path, int32_t path_len );

int32_t find_rw_index( substring child, int32_t curr_fd, int32_t temp_fd, bool wd_dirty );

substring find_parent_file_path( substring path );

substring find_child_file_name( substring path );

int32_t lookup( struct substring path, int32_t curr_fd, int32_t desired_fd, bool is_dir, bool wd_dirty );
