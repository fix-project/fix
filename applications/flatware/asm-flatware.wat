(module
  (import "c-flatware" "memory" (memory $flatware_mem 0))
  (import "wasi_command" "memory" (memory $program_mem 0))
  (import "wasi_command" "_start" (func $start))
  
  (func (export "get_i32_program") (param $offset i32) (result i32)
    (local.get $offset)
    (i32.load $program_mem)
  )
  (func (export "set_i32_program") (param $offset i32) (param $val i32)
    (i32.store $program_mem (local.get $offset) (local.get $val))
  )

  (func (export "get_i8_program") (param $offset i32) (result i32)
    (local.get $offset)
    (i32.load8_u $program_mem)
  )
  (func (export "set_i8_program") (param $offset i32) (param $val i32)
    (i32.store8 $program_mem (local.get $offset) (local.get $val))
  )

  (func (export "get_i32_flatware") (param $offset i32) (result i32)
    (local.get $offset)
    (i32.load $flatware_mem)
  )

  (func (export "get_i8_flatware") (param $flatware_pointer i32) (result i32)
    (local.get $flatware_pointer)
    (i32.load8_u $flatware_mem)
  )

  (func (export "set_i32_flatware") (param $flatware_pointer i32) (param $val i32)
    (i32.store $flatware_mem (local.get $flatware_pointer) (local.get $val))
  )

  (func (export "set_i8_flatware") (param $flatware_pointer i32) (param $val i32)
    (i32.store8 $flatware_mem (local.get $flatware_pointer) (local.get $val))
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
  (tag $exit)
  (func (export "exit") (throw $exit))
  (func (export "run-start") (try (do (call $start)) (catch $exit)))
)

