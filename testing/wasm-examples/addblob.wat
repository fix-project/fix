(module
  (type (;0;) (func (param externref)))
  (type (;1;) (func (param i32) (result externref)))
  (import "fixpoint" "attach_tree_ro_table_0" (func $attach_tree_ro_tree_0 (type 0)))
  (import "fixpoint" "attach_blob_ro_mem_0" (func $attach_blob_ro_mem_0 (type 0)))
  (import "fixpoint" "attach_blob_ro_mem_1" (func $attach_blob_ro_mem_1 (type 0)))
  (import "fixpoint" "create_blob_rw_mem_0" (func $create_blob_rw_mem_0  (type 1)))
  (memory $memzero 0)
  (memory $memone 0)
  (memory $memtwo 1)
  (table $ro_table 0 externref)
  (table $ro_handles 16 externref)
  (table $rw_handles 16 externref)
  (func (export "_fixpoint_apply") (param $encode externref) (result externref)
    (call $attach_tree_ro_tree_0 (local.get $encode))
    (table.set $ro_handles (i32.const 1) (table.get $ro_table (i32.const 2)))
    (table.set $ro_handles (i32.const 2) (table.get $ro_table (i32.const 3)))
    (call $attach_blob_ro_mem_0 (table.get $ro_handles (i32.const 1))) 
    (call $attach_blob_ro_mem_1 (table.get $ro_handles (i32.const 2)))
    i32.const 0
    (i32.add (i32.load 0 (i32.const 0)) (i32.load 1 (i32.const 0)))
    i32.store $memtwo
    (table.set $rw_handles (i32.const 0) (call $create_blob_rw_mem_0 (i32.const 4))) 
    (table.get $rw_handles (i32.const 0))
  )
  (table (;0;) 1 1 funcref)
  (export "ro_mem_0" (memory $memzero))
  (export "ro_mem_1" (memory $memone))
  (export "rw_mem_0" (memory $memtwo))
  (export "ro_table_0" (table $ro_table)) 
) 