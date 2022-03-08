#include "addblob_fixpoint.h"

extern void fixpoint_get_tree_entry( void*, uint32_t, uint32_t, uint32_t );
void get_tree_entry(Z_addblob_module_instance_t* module_instance, uint32_t src_ro_handle, uint32_t entry_num, uint32_t target_ro_handle) {
  fixpoint_get_tree_entry(module_instance, src_ro_handle, entry_num, target_ro_handle);
}
void (*WASM_RT_ADD_PREFIX(Z_envZ_get_tree_entryZ_viii))(Z_addblob_module_instance_t *, u32, u32, u32) = &get_tree_entry;

extern void fixpoint_attach_blob( void*, wasm_rt_memory_t*, uint32_t );
void attach_blob_0(Z_addblob_module_instance_t* module_instance, uint32_t ro_handle) {
  wasm_rt_memory_t* ro_mem = Z_ro_mem_0(module_instance);
  fixpoint_attach_blob(module_instance, ro_mem, ro_handle);
}

void attach_blob_1(Z_addblob_module_instance_t* module_instance, uint32_t ro_handle) {
  wasm_rt_memory_t* ro_mem = Z_ro_mem_1(module_instance);
  fixpoint_attach_blob(module_instance, ro_mem, ro_handle);
}

void attach_blob(Z_addblob_module_instance_t* module_instance, uint32_t ro_mem_num, uint32_t ro_handle) {
  if (ro_mem_num == 0) {
    attach_blob_0(module_instance, ro_handle);
    return;
  }
  if (ro_mem_num == 1) {
    attach_blob_1(module_instance, ro_handle);
    return;
  }
  wasm_rt_trap(WASM_RT_TRAP_OOB);
}
void (*WASM_RT_ADD_PREFIX(Z_envZ_attach_blobZ_vii))(Z_addblob_module_instance_t *, u32, u32) = &attach_blob;

extern void fixpoint_detach_mem( void*, wasm_rt_memory_t*, uint32_t );
void detach_mem_0(Z_addblob_module_instance_t* module_instance, uint32_t rw_handle) {
  wasm_rt_memory_t* rw_mem = Z_rw_mem_0(module_instance);
  fixpoint_detach_mem(module_instance, rw_mem, rw_handle);
}

void detach_mem(Z_addblob_module_instance_t* module_instance, uint32_t rw_mem_num, uint32_t rw_handle) {
  if (rw_mem_num == 0) {
    detach_mem_0(module_instance, rw_handle);
    return;
  }
  wasm_rt_trap(WASM_RT_TRAP_OOB);
}
void (*WASM_RT_ADD_PREFIX(Z_envZ_detach_memZ_vii))(Z_addblob_module_instance_t *, u32, u32) = &detach_mem;

extern void fixpoint_freeze_blob( void*, uint32_t, uint32_t, uint32_t );
void freeze_blob(Z_addblob_module_instance_t* module_instance, uint32_t rw_handle, uint32_t size, uint32_t ro_handle) {
  fixpoint_freeze_blob(module_instance, rw_handle, size, ro_handle);
}
void (*WASM_RT_ADD_PREFIX(Z_envZ_freeze_blobZ_viii))(Z_addblob_module_instance_t *, u32, u32, u32) = &freeze_blob;

extern void fixpoint_designate_output( void*, uint32_t );
void designate_output(Z_addblob_module_instance_t* module_instance, uint32_t ro_handle) {
  fixpoint_designate_output(module_instance, ro_handle);
}
void (*WASM_RT_ADD_PREFIX(Z_envZ_designate_outputZ_vi))(Z_addblob_module_instance_t *, u32) = &designate_output;

void executeProgram() {
  Z_addblob_module_instance_t instance;
  init(&instance);
  Z__fixpoint_applyZ_vv(&instance);
}
