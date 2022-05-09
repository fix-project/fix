#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef char __attribute__((address_space(10)))* externref;

extern void memory_copy_rw_0(const void*, size_t) __attribute( ( import_module("helper"), import_name("memory_copy_rw_0") ) );
extern externref detach_mem_rw_mem_0(void) __attribute( ( import_module("fixpoint"), import_name("detach_mem_rw_mem_0") ) );
extern externref freeze_blob (externref, uint32_t) __attribute( ( import_module("fixpoint"), import_name("freeze_blob") ) );
extern void designate_output(externref) __attribute( ( import_module("helper"), import_name("memory_copy_rw_0") ) );
void proc_exit( uint32_t rval ) __attribute( ( export_name("proc_exit") ) );

void proc_exit(uint32_t rval) {
	externref a;
	externref b;
	// copying rval to rw_memories[0]; 
	memory_copy_rw_0(&rval, sizeof(rval));
	// detach memory
	a = detach_mem_rw_mem_0();
	// freeze blob
	b = freeze_blob(a, sizeof(rval));
	designate_output(b);
}

