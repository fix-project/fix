(module
  (import "fixpoint" "attach_tree_ro_table_0" (func $attach_tree_ro_tree_0 (param externref)))
  (import "fixpoint" "attach_blob_ro_mem_0" (func $attach_blob_ro_mem_0 (param externref)))
  (import "fixpoint" "attach_blob_ro_mem_1" (func $attach_blob_ro_mem_1 (param externref)))
  (import "fixpoint" "create_blob_rw_mem_0" (func $create_blob_rw_mem_0 (param i32) (result externref)))
  (import "fixpoint" "create_blob_rw_mem_1" (func $create_blob_rw_mem_1 (param i32) (result externref)))
  (import "fixpoint" "create_tree_rw_table_0" (func $create_tree_rw_table_0 (param i32) (result externref)))
  (import "fixpoint" "create_tag" (func $create_tag (param externref externref) (result externref)))
  (import "fixpoint" "pin" (func $pin (param externref externref) (result i32)))
  (memory $ro_mem_0 (export "ro_mem_0") 0)
  (memory $ro_mem_1 (export "ro_mem_1") 0)
  (memory $rw_mem_0 (export "rw_mem_0") 1)
  (memory (export "rw_mem_1") (data "+"))
  (table $ro_table (export "ro_table_0") 0 externref)
  (table $rw_table_0 (export "rw_table_0") 3 externref)
  (func (export "_fixpoint_apply")
    (param $encode externref)
    (result externref)
    (local $result externref)
    (local $infotree externref)
    (call $attach_tree_ro_tree_0 (local.get $encode))
    (call $attach_blob_ro_mem_0 (table.get $ro_table (i32.const 2))) 
    (call $attach_blob_ro_mem_1 (table.get $ro_table (i32.const 3)))
    i32.const 0
    (i32.add (i32.load 0 (i32.const 0)) (i32.load 1 (i32.const 0)))
    i32.store $rw_mem_0
    (local.set $result (call $create_blob_rw_mem_0 (i32.const 4)))

    ;; Create tracing info
    (table.set $rw_table_0 (i32.const 0) (table.get $ro_table (i32.const 2)))
    (table.set $rw_table_0 (i32.const 1) (call $create_blob_rw_mem_1 (i32.const 1)))
    (table.set $rw_table_0 (i32.const 2) (table.get $ro_table (i32.const 3)))
    (local.set $infotree (call $create_tree_rw_table_0 (i32.const 3)))

    (local.get $result)
    (call $create_tag (local.get $result) (local.get $infotree))
    call $pin
    drop

    (local.get $result)
  )
)
