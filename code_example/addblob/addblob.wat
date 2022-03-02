(module
  (type (;0;) (func (param i32 i32 i32)))
  (type (;1;) (func (param i32 i32)))
  (type (;2;) (func (param i32)))
  (import "env" "get_tree_entry" (func $get_tree_entry (type 0)))
  (import "env" "attach_blob" (func $attach_blob (type 1)))
  (import "env" "detach_mem" (func $detach_mem (type 1)))
  (import "env" "freeze_blob" (func $freeze_blob (type 0)))
  (import "env" "designate_output" (func $designate_output (type 2)))
  (memory $memzero 0)
  (memory $memone 0)
  (memory $memtwo 0)
  (func (export "_fixpoint_apply")
    i32.const 0
    i32.const 2
    i32.const 1
    call $get_tree_entry
    i32.const 0
    i32.const 2
    i32.const 3
    call $get_tree_entry
    i32.const 1
    i32.const 0
    call $attach_blob
    i32.const 2
    i32.const 1
    call $attach_blob
    i32.const 1
    memory.grow $memtwo
    drop
    i32.const 0
    i32.const 0
    i32.load $memzero
    i32.const 0
    i32.load $memone
    i32.add
    i32.store $memtwo
    i32.const 0
    i32.const 0
    call $detach_mem
    i32.const 0
    i32.const 2
    i32.const 0
    call $freeze_blob
    i32.const 0
    call $designate_output
  )
  (table (;0;) 1 1 funcref)
  (export "ro_mem_0" (memory $memzero))
  (export "ro_mem_1" (memory $memone))
  (export "rw_mem_0" (memory $memtwo))
)
