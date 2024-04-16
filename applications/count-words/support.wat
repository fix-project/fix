(module
  (import "fixpoint_storage" "ro_mem_0" (memory $ro_mem_0 0))
  (import "fixpoint_storage" "rw_mem_0" (memory $rw_mem_0 0))
  (import "fixpoint_storage" "ro_mem_1" (memory $ro_mem_1 1))
  (import "fixpoint_storage" "rw_mem_1" (memory $rw_mem_1 1))
  (import "wasi_command" "memory" (memory $program_mem 0))
  (func (export "ro_mem_0_to_program_mem") (param $dest i32) (param $src i32) (param $len i32)
        (memory.copy $ro_mem_0 $program_mem
          (local.get $dest)
          (local.get $src)
          (local.get $len)))
  (func (export "program_mem_to_rw_mem_0") (param $dest i32) (param $src i32) (param $len i32)
        (memory.copy $program_mem $rw_mem_0
          (local.get $dest)
          (local.get $src)
          (local.get $len)))
  (func (export "ro_mem_1_to_program_mem") (param $dest i32) (param $src i32) (param $len i32)
        (memory.copy $ro_mem_1 $program_mem
          (local.get $dest)
          (local.get $src)
          (local.get $len)))
  (func (export "program_mem_to_rw_mem_1") (param $dest i32) (param $src i32) (param $len i32)
        (memory.copy $program_mem $rw_mem_1
          (local.get $dest)
          (local.get $src)
          (local.get $len))))
