#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef char __attribute__((address_space(10)))* externref;

typedef struct filedesc {
  uint32_t offset;
} filedesc;

static struct filedesc stdout = {.offset = 0};

extern void memory_copy_rw_0(const void*, size_t) __attribute( ( import_module("helper"), import_name("memory_copy_rw_0") ) );
extern void memory_copy_rw_1(const void*, size_t) __attribute( ( import_module("helper"), import_name("memory_copy_rw_1") ) );
extern void program_memory_copy_rw_1(uint32_t, uint32_t) __attribute( ( import_module("helper"), import_name("program_memory_copy_rw_1") ) );
extern externref create_blob_rw_mem_0(uint32_t) __attribute( ( import_module("fixpoint"), import_name("create_blob_rw_mem_0") ) );
extern externref create_blob_rw_mem_1(uint32_t) __attribute( ( import_module("fixpoint"), import_name("create_blob_rw_mem_1") ) );
extern externref create_tree_rw_table_0(uint32_t) __attribute( ( import_module("fixpoint"), import_name("create_tree_rw_table_0") ) );
extern void designate_output(externref) __attribute( ( import_module("helper"), import_name("designate_output") ) );
extern uint32_t get_i32( uint32_t ) __attribute( ( import_module("helper"), import_name("get_i32")));
extern void start( void ) __attribute( ( import_module("sloth"), import_name("_start")));
extern void set_return( uint32_t, externref ) __attribute( ( import_module("helper"), import_name("set_return")));
extern void fixpoint_exit(externref) __attribute( ( import_module("fixpoint"), import_name("exit") ) );


void proc_exit( uint32_t rval ) __attribute( ( export_name("proc_exit") ) );
uint32_t fd_close ( int fd ) __attribute( ( export_name("fd_close") ) );
uint32_t fd_fdstat_get ( uint32_t fd, uint32_t res ) __attribute ( ( export_name("fd_fdstat_get") ) );
uint32_t fd_seek ( uint32_t fd, int64_t offset, uint32_t whence, uint32_t file_size ) __attribute ( ( export_name("fd_seek") ) ); // not used atm
uint32_t fd_write ( uint32_t fd, uint32_t vec, uint32_t vec_len, uint32_t retptr0 ) __attribute ( ( export_name("fd_write") ) );
externref flatware_exit( void );
externref fixpoint_apply( externref encode ) __attribute( ( export_name("_fixpoint_apply")) );

void proc_exit(uint32_t rval) {
  externref a;
  // copying rval to rw_memories[0];
  memory_copy_rw_0(&rval, sizeof(rval));
  // detach memory
  a = create_blob_rw_mem_0(sizeof(rval));
  designate_output(a);
  flatware_exit();
}

uint32_t fd_close(__attribute__((unused)) int fd) {

  return 0;
}

uint32_t fd_fdstat_get(__attribute__((unused)) uint32_t fd, __attribute__((unused)) uint32_t res) {
  return 0;
}

uint32_t fd_seek(__attribute__((unused)) uint32_t fd, __attribute__((unused)) int64_t offset, __attribute__((unused)) uint32_t whence, __attribute__((unused)) uint32_t file_size) {
  return 0;
}

uint32_t fd_write(__attribute__((unused)) uint32_t fd, uint32_t iovs, __attribute__((unused)) uint32_t iovs_len, __attribute__((unused)) uint32_t retptr0) {
  uint32_t offset = get_i32(iovs);
  uint32_t size = get_i32(iovs + 4);
  program_memory_copy_rw_1(offset, size);
  stdout.offset += size;
  return size;
}

externref fixpoint_apply(__attribute__((unused)) externref encode) {
  uint32_t zero = 0;
  start();
  memory_copy_rw_0(&zero, sizeof(zero));
  set_return(0, create_blob_rw_mem_0(sizeof(zero)));
  set_return(1, create_blob_rw_mem_1(stdout.offset));
  return create_tree_rw_table_0(2);
}

externref flatware_exit() {
  externref b;
  b = create_blob_rw_mem_1(stdout.offset);
  set_return(1, b);
  fixpoint_exit(create_tree_rw_table_0(2));
  return b;
}

