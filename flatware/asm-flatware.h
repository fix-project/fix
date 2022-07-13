
#define DEFINE_RO_MEM_OPS( index )                                                                                 \
  extern void ro_##index##_to_program_memory( int32_t, int32_t, int32_t )                                          \
    __attribute__( ( import_module( "asm-flatware" ) ) );                                                          \
  extern void ro_##index##_to_flatware_memory( int32_t, const void*, int32_t )                                     \
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

DEFINE_RO_MEM_OPS( 0 )
DEFINE_RO_MEM_OPS( 1 )
DEFINE_RW_MEM_OPS( 0 )
DEFINE_RW_MEM_OPS( 1 )
DEFINE_RO_TABLE_OPS( 0 )
DEFINE_RW_TABLE_OPS( 0 )
