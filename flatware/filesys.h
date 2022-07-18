#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

bool is_dir( int32_t ro_table_index );

const char* get_name( int32_t ro_table_index, int32_t file_index );

uint64_t get_permissions( int32_t ro_table_index, int32_t file_index );

externref get_content( int32_t ro_table_index, int32_t file_index );

int32_t find_file( int32_t path, int32_t path_len, int32_t desired_fd );
