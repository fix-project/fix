(module
  (import "c-flatware" "memory" (memory $flatware_mem 0))
  (import "fixpoint_storage" "rw_mem_0" (memory $rw_mem_0 0))
  (import "fixpoint_storage" "rw_mem_1" (memory $rw_mem_1 0))
  (import "fixpoint_storage" "rw_mem_2" (memory $rw_mem_2 0))
  (import "fixpoint_storage" "ro_mem_0" (memory $ro_mem_0 0))
  (import "fixpoint_storage" "ro_mem_1" (memory $ro_mem_1 0))
  (import "fixpoint_storage" "ro_mem_2" (memory $ro_mem_2 0))
  (import "fixpoint_storage" "ro_mem_3" (memory $ro_mem_3 0))
  
  (import "wasi_command" "memory" (memory $program_mem 0))
  (import "wasi_command" "_start" (func $start))
  
  (func (export "get_i32_program") (param $offset i32) (result i32)
    (local.get $offset)
    (i32.load $program_mem)
  )
  (func (export "set_i32_program") (param $offset i32) (param $val i32)
    (i32.store $program_mem (local.get $offset) (local.get $val))
  )

  (func (export "get_i32_flatware") (param $offset i32) (result i32)
    (local.get $offset)
    (i32.load $flatware_mem)
  )

  (func (export "set_i32_flatware") (param $flatware_pointer i32) (param $val i32)
    (i32.store $flatware_mem (local.get $flatware_pointer) (local.get $val))
  ) 
  
  (func (export "memory_copy_program") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $flatware_mem $program_mem
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )
  (func (export "program_memory_to_flatware_memory") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $program_mem $flatware_mem
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )

  (func (export "ro_mem_0_to_flatware_memory") (param $flatware_offset i32) (param $ro_offset i32) (param $len i32)
    (memory.copy $ro_mem_0 $flatware_mem
    (local.get $flatware_offset)
    (local.get $ro_offset)
    (local.get $len))
  )
  (func (export "ro_mem_1_to_flatware_memory") (param $flatware_offset i32) (param $ro_offset i32) (param $len i32)
    (memory.copy $ro_mem_1 $flatware_mem
    (local.get $flatware_offset)
    (local.get $ro_offset)
    (local.get $len))
  )
  (func (export "ro_mem_2_to_flatware_memory") (param $flatware_offset i32) (param $ro_offset i32) (param $len i32)
    (memory.copy $ro_mem_2 $flatware_mem
    (local.get $flatware_offset)
    (local.get $ro_offset)
    (local.get $len))
  )
  (func (export "ro_mem_3_to_flatware_memory") (param $flatware_offset i32) (param $ro_offset i32) (param $len i32)
    (memory.copy $ro_mem_3 $flatware_mem
    (local.get $flatware_offset)
    (local.get $ro_offset)
    (local.get $len))
  )
  (func (export "ro_mem_0_to_program_memory") (param $program_offset i32) (param $ro_offset i32) (param $len i32)
    (memory.copy $ro_mem_0 $program_mem
    (local.get $program_offset)
    (local.get $ro_offset)
    (local.get $len))
  )
  (func (export "ro_mem_1_to_program_memory") (param $program_offset i32) (param $ro_offset i32) (param $len i32)
    (memory.copy $ro_mem_1 $program_mem
    (local.get $program_offset)
    (local.get $ro_offset)
    (local.get $len))
  )
  (func (export "ro_mem_2_to_program_memory") (param $program_offset i32) (param $ro_offset i32) (param $len i32)
    (memory.copy $ro_mem_2 $program_mem
    (local.get $program_offset)
    (local.get $ro_offset)
    (local.get $len))
  )
  (func (export "ro_mem_3_to_program_memory") (param $program_offset i32) (param $ro_offset i32) (param $len i32)
    (memory.copy $ro_mem_3 $program_mem
    (local.get $program_offset)
    (local.get $ro_offset)
    (local.get $len))
  )
  (func (export "program_memory_to_rw_0") (param $rw_offset i32) (param $program_offset i32) (param $len i32)
    (memory.copy $program_mem $rw_mem_0
    (local.get $rw_offset)
    (local.get $program_offset)
    (local.get $len))
  )
  (func (export "program_memory_to_rw_1") (param $rw_offset i32) (param $program_offset i32) (param $len i32)
    (memory.copy $program_mem $rw_mem_1
    (local.get $rw_offset)
    (local.get $program_offset)
    (local.get $len))
  )
  (func (export "program_memory_to_rw_2") (param $rw_offset i32) (param $program_offset i32) (param $len i32)
    (memory.copy $program_mem $rw_mem_2
    (local.get $rw_offset)
    (local.get $program_offset)
    (local.get $len))
  )
  (func (export "flatware_memory_to_rw_0") (param $rw_offset i32) (param $flatware_offset i32) (param $len i32)
    (memory.copy $flatware_mem $rw_mem_0
    (local.get $rw_offset)
    (local.get $flatware_offset)
    (local.get $len))
  )
  (func (export "flatware_memory_to_rw_1") (param $rw_offset i32) (param $flatware_offset i32) (param $len i32)
    (memory.copy $flatware_mem $rw_mem_1
    (local.get $rw_offset)
    (local.get $flatware_offset)
    (local.get $len))
  )
  (func (export "flatware_memory_to_rw_2") (param $rw_offset i32) (param $flatware_offset i32) (param $len i32)
    (memory.copy $flatware_mem $rw_mem_2
    (local.get $rw_offset)
    (local.get $flatware_offset)
    (local.get $len))
  )

  (tag $exit)
  (func (export "exit") (throw $exit))
  (func (export "run-start") (try (do (call $start)) (catch $exit)))
)

