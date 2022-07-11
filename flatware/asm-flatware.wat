(module
  (import "c-flatware" "memory" (memory $tmem 0))
  (import "wasi_command" "memory" (memory $smem 0))
  (import "wasi_command" "_start" (func $start))
  (memory $mymem0 (export "rw_mem_0") 1)
  (memory $mymem1 (export "rw_mem_1") 1)
  (memory $argmem (export "ro_mem_0") 0)
  (memory $fs (export "ro_mem_1") 0)
  (table $input (export "ro_table_0") 0 externref)
  (table $return (export "rw_table_0") 2 externref)
  (func (export "memory_copy_rw_0") (param $ptr i32) (param $len i32)
  	(memory.copy $tmem $mymem0
		(i32.const 0)
		(local.get $ptr)
		(local.get $len))
  )
  (func (export "memory_copy_rw_1") (param $ptr i32) (param $len i32)
  	(memory.copy $tmem $mymem1
		(i32.const 0)
		(local.get $ptr)
		(local.get $len))
  )
  (func (export "program_memory_copy_rw_0") (param $ptr i32) (param $len i32)
    (memory.copy $smem $mymem0
    (i32.const 0)
    (local.get $ptr)
    (local.get $len))
  )
  (func (export "program_memory_copy_rw_1") (param $ptr i32) (param $len i32)
    (memory.copy $smem $mymem1
    (i32.const 0)
    (local.get $ptr)
    (local.get $len))
  )
  
  (func (export "get_program_i32") (param $offset i32) (result i32)
    (local.get $offset)
    (i32.load $smem)
  )
  (func (export "set_program_i32") (param $offset i32) (param $val i32)
    (i32.store $smem (local.get $offset) (local.get $val))
  )
  
  (func (export "memory_copy_program") (param $offset i32) (param $ptr i32) (param $len i32)
    (memory.copy $tmem $smem
    (local.get $offset)
    (local.get $ptr)
    (local.get $len))
  )
  (func (export "memory_copy_program_ro_0") (param $ptr i32) (param $len i32)
    (memory.copy $argmem $smem
    (local.get $ptr)
    (i32.const 0)
    (local.get $len))
  )
  ;; need memory_copy_program_ro_1, copy from memory to program memory
  (func (export "memory_copy_program_ro_1") (param $ptr i32) (param $len i32)
    (memory.copy $fs $smem
    (local.get $ptr)
    (i32.const 0)
    (local.get $len))
  )
  (func (export "get_from_return") (param $index i32) (result externref)
    (table.get $return (local.get $index))
  )
  (func (export "set_return") (param $index i32) (param $val externref)
    (table.set $return (local.get $index) (local.get $val))
  )

  (func (export "args_num") (result i32)
    (table.size $input)
  )
  (func (export "get_arg") (param $index i32) (result externref)
    (table.get $input (local.get $index))
  )

  (tag $exit)
  (func (export "exit") (throw $exit))
  (func (export "run-start") (try (do (call $start)) (catch $exit)))
)

