// This file was automatically generated by generate.py
// Imports wasm tables, memories, and helper functions from                     "fixpoint_storage" and imports fixpoint api functions.

#include <stdint.h>
typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

const static int NUM_RO_MEM = 4;
const static int NUM_RO_TABLE = 8;
const static int NUM_RW_MEM = 3;
const static int NUM_RW_TABLE = 3;

// ro_table_0 imports 
externref get_ro_table_0( int32_t index )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_ro_table_0" ) ) )
;int32_t size_ro_table_0(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_ro_table_0" ) ) );
void attach_tree_ro_table_0(externref)
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_tree_ro_table_0" ) ) );
externref get_attached_tree_ro_table_0(void)
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_attached_tree_ro_table_0" ) ) );

// ro_table_1 imports 
externref get_ro_table_1( int32_t index )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_ro_table_1" ) ) )
;int32_t size_ro_table_1(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_ro_table_1" ) ) );
void attach_tree_ro_table_1(externref)
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_tree_ro_table_1" ) ) );
externref get_attached_tree_ro_table_1(void)
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_attached_tree_ro_table_1" ) ) );

// ro_table_2 imports 
externref get_ro_table_2( int32_t index )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_ro_table_2" ) ) )
;int32_t size_ro_table_2(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_ro_table_2" ) ) );
void attach_tree_ro_table_2(externref)
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_tree_ro_table_2" ) ) );
externref get_attached_tree_ro_table_2(void)
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_attached_tree_ro_table_2" ) ) );

// ro_table_3 imports 
externref get_ro_table_3( int32_t index )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_ro_table_3" ) ) )
;int32_t size_ro_table_3(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_ro_table_3" ) ) );
void attach_tree_ro_table_3(externref)
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_tree_ro_table_3" ) ) );
externref get_attached_tree_ro_table_3(void)
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_attached_tree_ro_table_3" ) ) );

// ro_table_4 imports 
externref get_ro_table_4( int32_t index )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_ro_table_4" ) ) )
;int32_t size_ro_table_4(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_ro_table_4" ) ) );
void attach_tree_ro_table_4(externref)
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_tree_ro_table_4" ) ) );
externref get_attached_tree_ro_table_4(void)
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_attached_tree_ro_table_4" ) ) );

// ro_table_5 imports 
externref get_ro_table_5( int32_t index )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_ro_table_5" ) ) )
;int32_t size_ro_table_5(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_ro_table_5" ) ) );
void attach_tree_ro_table_5(externref)
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_tree_ro_table_5" ) ) );
externref get_attached_tree_ro_table_5(void)
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_attached_tree_ro_table_5" ) ) );

// ro_table_6 imports 
externref get_ro_table_6( int32_t index )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_ro_table_6" ) ) )
;int32_t size_ro_table_6(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_ro_table_6" ) ) );
void attach_tree_ro_table_6(externref)
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_tree_ro_table_6" ) ) );
externref get_attached_tree_ro_table_6(void)
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_attached_tree_ro_table_6" ) ) );

// ro_table_7 imports 
externref get_ro_table_7( int32_t index )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_ro_table_7" ) ) )
;int32_t size_ro_table_7(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_ro_table_7" ) ) );
void attach_tree_ro_table_7(externref)
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_tree_ro_table_7" ) ) );
externref get_attached_tree_ro_table_7(void)
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_attached_tree_ro_table_7" ) ) );

// rw_table_0 imports 
void set_rw_table_0( int32_t index, externref pointer )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "set_rw_table_0" ) ) );
externref get_rw_table_0( int32_t index )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_rw_table_0" ) ) )
;int32_t grow_rw_table_0( int32_t size, externref pointer )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "grow_rw_table_0" ) ) );
int32_t size_rw_table_0(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_rw_table_0" ) ) );
externref create_tree_rw_table_0( int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_tree_rw_table_0" ) ) );

// rw_table_1 imports 
void set_rw_table_1( int32_t index, externref pointer )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "set_rw_table_1" ) ) );
externref get_rw_table_1( int32_t index )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_rw_table_1" ) ) )
;int32_t grow_rw_table_1( int32_t size, externref pointer )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "grow_rw_table_1" ) ) );
int32_t size_rw_table_1(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_rw_table_1" ) ) );
externref create_tree_rw_table_1( int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_tree_rw_table_1" ) ) );

// rw_table_2 imports 
void set_rw_table_2( int32_t index, externref pointer )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "set_rw_table_2" ) ) );
externref get_rw_table_2( int32_t index )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_rw_table_2" ) ) )
;int32_t grow_rw_table_2( int32_t size, externref pointer )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "grow_rw_table_2" ) ) );
int32_t size_rw_table_2(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_rw_table_2" ) ) );
externref create_tree_rw_table_2( int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_tree_rw_table_2" ) ) );

// ro_mem_0 imports 
int32_t get_i32_ro_mem_0( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i32_ro_mem_0" ) ) );
int32_t get_i8_ro_mem_0( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i8_ro_mem_0" ) ) );
int32_t byte_size_ro_mem_0(void) __attribute__( ( import_module( "fixpoint" ),             import_name( "size_ro_mem_0" ) ) );
externref get_attached_blob_ro_mem_0(void)
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_attached_blob_ro_mem_0" ) ) );
void attach_blob_ro_mem_0(externref)
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_blob_ro_mem_0" ) ) );

// ro_mem_1 imports 
int32_t get_i32_ro_mem_1( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i32_ro_mem_1" ) ) );
int32_t get_i8_ro_mem_1( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i8_ro_mem_1" ) ) );
int32_t byte_size_ro_mem_1(void) __attribute__( ( import_module( "fixpoint" ),             import_name( "size_ro_mem_1" ) ) );
externref get_attached_blob_ro_mem_1(void)
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_attached_blob_ro_mem_1" ) ) );
void attach_blob_ro_mem_1(externref)
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_blob_ro_mem_1" ) ) );

// ro_mem_2 imports 
int32_t get_i32_ro_mem_2( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i32_ro_mem_2" ) ) );
int32_t get_i8_ro_mem_2( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i8_ro_mem_2" ) ) );
int32_t byte_size_ro_mem_2(void) __attribute__( ( import_module( "fixpoint" ),             import_name( "size_ro_mem_2" ) ) );
externref get_attached_blob_ro_mem_2(void)
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_attached_blob_ro_mem_2" ) ) );
void attach_blob_ro_mem_2(externref)
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_blob_ro_mem_2" ) ) );

// ro_mem_3 imports 
int32_t get_i32_ro_mem_3( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i32_ro_mem_3" ) ) );
int32_t get_i8_ro_mem_3( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i8_ro_mem_3" ) ) );
int32_t byte_size_ro_mem_3(void) __attribute__( ( import_module( "fixpoint" ),             import_name( "size_ro_mem_3" ) ) );
externref get_attached_blob_ro_mem_3(void)
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_attached_blob_ro_mem_3" ) ) );
void attach_blob_ro_mem_3(externref)
  __attribute__( ( import_module( "fixpoint" ), import_name( "attach_blob_ro_mem_3" ) ) );

// rw_mem_0 imports 
void set_i32_rw_mem_0( int32_t index, int32_t val )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "set_i32_rw_mem_0" ) ) );
void set_i8_rw_mem_0( int32_t index, int32_t val )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "set_i8_rw_mem_0" ) ) );
int32_t get_i32_rw_mem_0( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i32_rw_mem_0" ) ) );
int32_t get_i8_rw_mem_0( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i8_rw_mem_0" ) ) );
int32_t grow_rw_mem_0( int32_t size )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "grow_rw_mem_0" ) ) );
int32_t page_size_rw_mem_0(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_rw_mem_0" ) ) );
externref create_blob_rw_mem_0( int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_blob_rw_mem_0" ) ) );

// rw_mem_1 imports 
void set_i32_rw_mem_1( int32_t index, int32_t val )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "set_i32_rw_mem_1" ) ) );
void set_i8_rw_mem_1( int32_t index, int32_t val )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "set_i8_rw_mem_1" ) ) );
int32_t get_i32_rw_mem_1( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i32_rw_mem_1" ) ) );
int32_t get_i8_rw_mem_1( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i8_rw_mem_1" ) ) );
int32_t grow_rw_mem_1( int32_t size )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "grow_rw_mem_1" ) ) );
int32_t page_size_rw_mem_1(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_rw_mem_1" ) ) );
externref create_blob_rw_mem_1( int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_blob_rw_mem_1" ) ) );

// rw_mem_2 imports 
void set_i32_rw_mem_2( int32_t index, int32_t val )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "set_i32_rw_mem_2" ) ) );
void set_i8_rw_mem_2( int32_t index, int32_t val )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "set_i8_rw_mem_2" ) ) );
int32_t get_i32_rw_mem_2( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i32_rw_mem_2" ) ) );
int32_t get_i8_rw_mem_2( int32_t )     __attribute__( ( import_module( "fixpoint_storage" ), import_name( "get_i8_rw_mem_2" ) ) );
int32_t grow_rw_mem_2( int32_t size )
  __attribute__( ( import_module( "fixpoint_storage" ), import_name( "grow_rw_mem_2" ) ) );
int32_t page_size_rw_mem_2(void) __attribute__( ( import_module( "fixpoint_storage" ),             import_name( "size_rw_mem_2" ) ) );
externref create_blob_rw_mem_2( int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_blob_rw_mem_2" ) ) );

// ro table function arrays 
static externref (*get_ro_table_functions[]) (int32_t) = {get_ro_table_0,get_ro_table_1,get_ro_table_2,get_ro_table_3,get_ro_table_4,get_ro_table_5,get_ro_table_6,get_ro_table_7};
static int32_t (*size_ro_table_functions[])(void) = {size_ro_table_0,size_ro_table_1,size_ro_table_2,size_ro_table_3,size_ro_table_4,size_ro_table_5,size_ro_table_6,size_ro_table_7};
static externref (*get_attached_tree_functions[]) (void) = {get_attached_tree_ro_table_0,get_attached_tree_ro_table_1,get_attached_tree_ro_table_2,get_attached_tree_ro_table_3,get_attached_tree_ro_table_4,get_attached_tree_ro_table_5,get_attached_tree_ro_table_6,get_attached_tree_ro_table_7};
static void (*attach_tree_functions[]) (externref) = {attach_tree_ro_table_0,attach_tree_ro_table_1,attach_tree_ro_table_2,attach_tree_ro_table_3,attach_tree_ro_table_4,attach_tree_ro_table_5,attach_tree_ro_table_6,attach_tree_ro_table_7};

// rw table function arrays 
static externref (*get_rw_table_functions[]) (int32_t) = {get_rw_table_0,get_rw_table_1,get_rw_table_2};
static int32_t (*size_rw_table_functions[])(void) = {size_rw_table_0,size_rw_table_1,size_rw_table_2};
static int32_t (*grow_rw_table_functions[]) (int32_t, externref) = {grow_rw_table_0,grow_rw_table_1,grow_rw_table_2};
static void (*set_rw_table_functions[]) (int32_t, externref) = {set_rw_table_0,set_rw_table_1,set_rw_table_2};
static externref (*create_tree_functions[]) (int32_t) = {create_tree_rw_table_0,create_tree_rw_table_1,create_tree_rw_table_2};

// ro mem function arrays 
static int32_t (*get_i32_ro_mem_functions[]) (int32_t) = {get_i32_ro_mem_0,get_i32_ro_mem_1,get_i32_ro_mem_2,get_i32_ro_mem_3};
static int32_t (*get_i8_ro_mem_functions[]) (int32_t) = {get_i8_ro_mem_0,get_i8_ro_mem_1,get_i8_ro_mem_2,get_i8_ro_mem_3};
static int32_t (*byte_size_ro_mem_functions[])(void) = {byte_size_ro_mem_0,byte_size_ro_mem_1,byte_size_ro_mem_2,byte_size_ro_mem_3};
static externref (*get_attached_blob_functions[]) (void) = {get_attached_blob_ro_mem_0,get_attached_blob_ro_mem_1,get_attached_blob_ro_mem_2,get_attached_blob_ro_mem_3};
static void (*attach_blob_functions[]) (externref) = {attach_blob_ro_mem_0,attach_blob_ro_mem_1,attach_blob_ro_mem_2,attach_blob_ro_mem_3};

// rw mem function arrays 
static int32_t (*get_i32_rw_mem_functions[]) (int32_t) = {get_i32_rw_mem_0,get_i32_rw_mem_1,get_i32_rw_mem_2};
static int32_t (*get_i8_rw_mem_functions[]) (int32_t) = {get_i8_rw_mem_0,get_i8_rw_mem_1,get_i8_rw_mem_2};
static int32_t (*page_size_rw_mem_functions[])(void) = {page_size_rw_mem_0,page_size_rw_mem_1,page_size_rw_mem_2};
static int32_t (*grow_rw_mem_functions[]) (int32_t) = {grow_rw_mem_0,grow_rw_mem_1,grow_rw_mem_2};
static void (*set_i32_rw_mem_functions[]) (int32_t, int32_t) = {set_i32_rw_mem_0,set_i32_rw_mem_1,set_i32_rw_mem_2};
static void (*set_i8_rw_mem_functions[]) (int32_t, int32_t) = {set_i8_rw_mem_0,set_i8_rw_mem_1,set_i8_rw_mem_2};
static externref (*create_blob_functions[]) (int32_t) = {create_blob_rw_mem_0,create_blob_rw_mem_1,create_blob_rw_mem_2};

