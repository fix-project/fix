(module
  (import "c-flatware" "memory" (memory $flatware_mem 0))
  (import "wasi_command" "memory" (memory $program_mem 0))
  (import "wasi_command" "_start" (func $start))
  (memory $mymem0 (export "rw_mem_0") 1)
  (memory $mymem1 (export "rw_mem_1") 1)
  (memory $trace_mem (export "rw_mem_2") 1)
  (memory $argmem (export "ro_mem_0") 0)
  (memory $fs (export "ro_mem_1") 0)
  (table $input (export "ro_table_0") 0 externref)
  (table $ro_table_1 (export "ro_table_1") 0 externref)
  (table $return (export "rw_table_0") 3 externref)
  ;; rw_0
  (func (export "flatware_memory_to_rw_0") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $flatware_mem $mymem0
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )
  (func (export "program_memory_to_rw_0") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $program_mem $mymem0
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )
  ;; rw_1
  (func (export "flatware_memory_to_rw_1") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $flatware_mem $mymem1
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )
  (func (export "program_memory_to_rw_1") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $program_mem $mymem1
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )

  ;; rw_2: trace_mem
  (func (export "flatware_memory_to_rw_2") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $flatware_mem $trace_mem
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )
  (func (export "program_memory_to_rw_2") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $program_mem $trace_mem
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )
  
  (func (export "get_program_i32") (param $offset i32) (result i32)
    (local.get $offset)
    (i32.load $program_mem)
  )
  (func (export "set_program_i32") (param $offset i32) (param $val i32)
    (i32.store $program_mem (local.get $offset) (local.get $val))
  )
  
  (func (export "memory_copy_program") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $flatware_mem $program_mem
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )
  ;; ro_0
  (func (export "ro_0_to_program_memory") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $argmem $program_mem
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )
  (func (export "ro_0_to_flatware_memory") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $argmem $flatware_mem
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )
  ;; ro_1
  (func (export "ro_1_to_program_memory") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $fs $program_mem
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )
  (func (export "ro_1_to_flatware_memory") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $fs $flatware_mem
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )

  ;; ro_table_0
  (func (export "get_ro_table_0") (param $index i32) (result externref)
    (table.get $input (local.get $index))
  )
  (func (export "size_ro_table_0") (result i32)
    (table.size $input)
  )
  ;; ro_table_1
  (func (export "get_ro_table_1") (param $index i32) (result externref)
    (table.get $ro_table_1 (local.get $index))
  )
  (func (export "size_ro_table_1") (result i32)
    (table.size $ro_table_1)
  )
  ;; rw_table_0
  (func (export "set_rw_table_0") (param $index i32) (param $val externref) 
    (table.set $return (local.get $index) (local.get $val))
  )

  (tag $exit)
  (func (export "exit") (throw $exit))
  (func (export "run-start") (try (do (call $start)) (catch $exit)))
)

