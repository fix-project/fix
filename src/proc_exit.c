#include<stddef.h>

void proc_exit( int a ) __attribute( ( export_name( "proc_exit" ) ) );

void memory_copy_rw_0( const void* ptr, size_t len ) __attribute( ( import_module("helper"), import_name( "memory_copy_rw_0") ) );

void proc_exit( int a )
{
  memory_copy_rw_0(&a, sizeof(a));
}
