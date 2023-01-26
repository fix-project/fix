#include <cstdint>
#include <string>

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

externref create_blob_i32( uint32_t number )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_blob_i32" ) ) );

// rw_table_0
void set_rw_table_0( int32_t index, externref pointer )
  __attribute__( ( import_module( "map_memory" ), import_name( "set_rw_table_0" ) ) );
int32_t grow_rw_table_0( int32_t size, externref pointer )
  __attribute__( ( import_module( "map_memory" ), import_name( "grow_rw_table_0" ) ) );
externref create_tree_rw_table_0( int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_tree_rw_table_0" ) ) );

// rw_table_1
void set_rw_table_1( int32_t index, externref pointer )
  __attribute__( ( import_module( "map_memory" ), import_name( "set_rw_table_1" ) ) );
int32_t grow_rw_table_1( int32_t size, externref pointer )
  __attribute__( ( import_module( "map_memory" ), import_name( "grow_rw_table_0" ) ) );
externref create_tree_rw_table_1( int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_tree_rw_table_1" ) ) );

// ro_table_0
void attach_tree_ro_table_0( externref pointer )
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_tree_ro_table_0" ) ) );
externref get_ro_table_0( int32_t index )
  __attribute__( ( import_module( "map_memory" ), import_name( "get_ro_table_0" ) ) );

// rw_mem_0
int32_t grow_rw_mem_0( int32_t size )
  __attribute__( ( import_module( "map_memory" ), import_name( "grow_rw_mem_0" ) ) );
void set_rw_mem_0( int32_t index, int32_t val )
  __attribute__( ( import_module( "map_memory" ), import_name( "set_rw_mem_0" ) ) );
externref create_blob_rw_mem_0( int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_blob_rw_mem_0" ) ) );

// ro_mem_0
int32_t get_ro_mem_0( int32_t ) __attribute__( ( import_module( "map_memory" ), import_name( "get_ro_mem_0" ) ) );
void attach_blob_ro_mem_0( externref pointer )
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_blob_ro_mem_0" ) ) );

// thunk
externref create_thunk( externref pointer )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_thunk" ) ) );

// Map applies main_blob to all the elements in arr. Passes the element as the fourth
// value in the encode and arg_1 as the fifth.
externref map( externref resource_limits, externref main_blob, uint32_t arr[], uint32_t arr_size, uint32_t arg_1 )
{
  grow_rw_table_1( arr_size, resource_limits );

  externref arg_blob = create_blob_i32( arg_1 );
  for ( int i = 0; i < arr_size; i++ ) {
    set_rw_table_0( 0, resource_limits );
    set_rw_table_0( 1, main_blob );
    set_rw_table_0( 2, arg_blob );
    set_rw_table_0( 3, create_blob_i32( arr[i] ) );
    set_rw_table_1( i, create_thunk( create_tree_rw_table_0( 4 ) ) );
    grow_rw_table_0( 4, resource_limits );
  }
  return create_tree_rw_table_1( arr_size );
}
