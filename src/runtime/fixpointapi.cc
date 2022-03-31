#include "fixpointapi.hh"

namespace fixpoint {
void* init_module_instance( size_t instance_size )
{
  (void)instance_size;
  return NULL;
}

void get_tree_entry( void* module_instance, uint32_t src_ro_handle, uint32_t entry_num, uint32_t target_ro_handle )
{
  (void)module_instance;
  (void)src_ro_handle;
  (void)entry_num;
  (void)target_ro_handle;
}

void attach_blob( void* module_instance, wasm_rt_memory_t* target_memory, uint32_t ro_handle )
{
  (void)module_instance;
  (void)target_memory;
  (void)ro_handle;
}

void detach_mem( void* module_instance, wasm_rt_memory_t* target_memory, uint32_t rw_handle )
{
  (void)module_instance;
  (void)target_memory;
  (void)rw_handle;
}

void freeze_blob( void* module_instance, size_t size, uint32_t ro_handle )
{
  (void)module_instance;
  (void)size;
  (void)ro_handle;
}

void designate_output( void* module_instance, uint32_t ro_handle )
{
  (void)module_instance;
  (void)ro_handle;
}
}
