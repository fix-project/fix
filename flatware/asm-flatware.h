#include "api.h"

#define DEFINE_RO_MEM_OPS( index )                                                                                 \
  extern externref get_ro_mem_##index( int32_t ) __attribute__( ( import_module( "asm-flatware" ) ) );             \
  extern void ro_##index##_to_program_memory( int32_t, int32_t, int32_t )                                          \
    __attribute__( ( import_module( "asm-flatware" ) ) );                                                          \
  extern void ro_##index##_to_flatware_memory( const void*, int32_t, int32_t )                                     \
    __attribute__( ( import_module( "asm-flatware" ) ) );                                                          \
  extern void attach_blob_ro_mem_##index( externref ) __attribute__( ( import_module( "fixpoint" ) ) );            \
  extern int32_t size_ro_mem_##index( void ) __attribute__( ( import_module( "fixpoint" ) ) );

#define DEFINE_RW_MEM_OPS( index )                                                                                 \
  extern void program_memory_to_rw_##index( int32_t, int32_t, int32_t )                                            \
    __attribute__( ( import_module( "asm-flatware" ) ) );                                                          \
  extern void flatware_memory_to_rw_##index( int32_t, const void*, int32_t )                                       \
    __attribute__( ( import_module( "asm-flatware" ) ) );                                                          \
  extern externref create_blob_rw_mem_##index( int32_t ) __attribute__( ( import_module( "fixpoint" ) ) );

#define DEFINE_RO_TABLE_OPS( index )                                                                               \
  extern externref get_ro_table_##index( int32_t ) __attribute__( ( import_module( "asm-flatware" ) ) );           \
  extern int32_t size_ro_table_##index( void ) __attribute__( ( import_module( "asm-flatware" ) ) );               \
  extern void attach_tree_ro_table_##index( externref ) __attribute__( ( import_module( "fixpoint" ) ) );

#define DEFINE_RW_TABLE_OPS( index )                                                                               \
  extern void set_rw_table_##index( int32_t, externref ) __attribute__( ( import_module( "asm-flatware" ) ) );     \
  extern externref create_tree_rw_table_##index( int32_t ) __attribute__( ( import_module( "fixpoint" ) ) );

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

extern externref create_blob_i32( int32_t )
  __attribute( ( import_module( "fixpoint" ), import_name( "create_blob_i32" ) ) );
extern int32_t value_type( externref ) __attribute( ( import_module( "fixpoint" ), import_name( "value_type" ) ) );

extern void memory_copy_program( int32_t, const void*, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_program" ) ) );
extern void program_memory_to_flatware_memory( const void*, int32_t, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "program_memory_to_flatware_memory" ) ) );
extern int32_t get_program_i32( int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "get_program_i32" ) ) );
extern void set_program_i32( int32_t, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "set_program_i32" ) ) );

extern void run_start( void ) __attribute( ( import_module( "asm-flatware" ), import_name( "run-start" ) ) );
__attribute( ( noreturn ) ) void flatware_exit( void )
  __attribute( ( import_module( "asm-flatware" ), import_name( "exit" ) ) );

DEFINE_RO_MEM_OPS( 0 )
DEFINE_RO_MEM_OPS( 1 )
DEFINE_RO_MEM_OPS( 2 )

DEFINE_RW_MEM_OPS( 0 )
DEFINE_RW_MEM_OPS( 1 )
DEFINE_RW_MEM_OPS( 2 )

DEFINE_RO_TABLE_OPS( 0 )
DEFINE_RO_TABLE_OPS( 1 )
DEFINE_RO_TABLE_OPS( 2 )
DEFINE_RO_TABLE_OPS( 3 )
DEFINE_RO_TABLE_OPS( 4 )
DEFINE_RW_TABLE_OPS( 0 )

static externref ( *get_ro_table_functions[] )( int32_t )
  = { get_ro_table_0, get_ro_table_1, get_ro_table_2, get_ro_table_3, get_ro_table_4 };
static void ( *attach_tree_ro_table_functions[] )( externref ) = { attach_tree_ro_table_0,
                                                                   attach_tree_ro_table_1,
                                                                   attach_tree_ro_table_2,
                                                                   attach_tree_ro_table_3,
                                                                   attach_tree_ro_table_4 };
externref get_ro_table( int32_t table_index, int32_t index ) __attribute__( ( __export_name__( "get_ro_table" ) ) );
void attach_tree_ro_table( int32_t table_index, externref name )
  __attribute__( ( __export_name__( "attach_tree_ro_table" ) ) );
void write_trace( const char* str, int32_t len );
