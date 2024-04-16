(module
  (import "fixpoint_storage" "ro_mem_0" (memory $ro_mem_0 0))
  (import "wasi_command" "memory" (memory $program_mem 0))
  (func (export "ro_mem_0_to_program_mem") (param $dest i32) (param $src i32) (param $len i32)
        (memory.copy $ro_mem_0 $program_mem
          (local.get $dest)
          (local.get $src)
          (local.get $len))))
