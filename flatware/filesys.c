#include "filesys.h"
#include "asm-flatware.h"

#include <string.h>

bool is_dir( __attribute__( ( unused ) ) int32_t ro_table_index )
{
  return false;
}

const char* get_name( __attribute__( ( unused ) ) int32_t ro_table_index,
                      __attribute__( ( unused ) ) int32_t file_index )
{
  return NULL;
}

uint64_t get_permissions( __attribute__( ( unused ) ) int32_t ro_table_index,
                          __attribute__( ( unused ) ) int32_t file_index )
{
  return 0;
}

externref get_content( __attribute__( ( unused ) ) int32_t ro_table_index,
                       __attribute__( ( unused ) ) int32_t file_index )
{
  return get_ro_table_0( 0 );
}

int32_t find_file( __attribute__( ( unused ) ) int32_t path,
                   __attribute__( ( unused ) ) int32_t path_len,
                   __attribute__( ( unused ) ) int32_t desired_fd )
{
  return 0;
}
