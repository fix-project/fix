#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct substring
{
  void* ptr;
  size_t len;
};

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

int32_t create_file( int32_t path, int32_t path_len, int32_t curr_fd, int32_t desired_fd );

bool is_dir( int32_t ro_table_index );

struct substring get_name( int32_t ro_table_index );

uint64_t get_permissions( int32_t ro_table_index );

externref get_content( int32_t ro_table_index );

int32_t find_local_file( struct substring path, int32_t curr_fd, bool should_be_dir, int32_t desired_fd );

int32_t find_file( int32_t path, int32_t path_len, int32_t curr_fd, int32_t desired_fd );

int32_t lookup( struct substring path, int32_t curr_fd, int32_t desired_fd );
