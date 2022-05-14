#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef char __attribute__((address_space(10)))* externref;

extern void memory_copy_rw_0(const void*, size_t) __attribute( ( import_module("helper"), import_name("memory_copy_rw_0") ) );
extern externref create_blob_rw_mem_0(uint32_t) __attribute( ( import_module("fixpoint"), import_name("create_blob_rw_mem_0") ) );
extern void designate_output(externref) __attribute( ( import_module("helper"), import_name("designate_output") ) );
extern void flatware_exit( void ) __attribute( ( import_module("helper"), import_name("flatware_exit")));

void proc_exit( uint32_t rval ) __attribute( ( export_name("proc_exit") ) );

void proc_exit(uint32_t rval) {
  externref a;
  // copying rval to rw_memories[0];
  memory_copy_rw_0(&rval, sizeof(rval));
  // detach memory
  a = create_blob_rw_mem_0(sizeof(rval));
  designate_output(a);
  flatware_exit();
}

