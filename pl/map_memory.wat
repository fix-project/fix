;; Module defining memory and memory interface for the map function.

(module
  (table $rw_table_0 (export "rw_table_0") 1000 externref)
  (table $rw_table_1 (export "rw_table_1") 10 externref)
  (table $ro_table_0 (export "ro_table_0") 0 externref) 
  (memory $rw_mem_0  (export "rw_mem_0")   1)
  (memory $ro_mem_0  (export "ro_mem_0")   0)
  
  (func (export "set_rw_table_0") (param $index i32) (param $val externref) 
    (table.set $rw_table_0 (local.get $index) (local.get $val))
  )

  (func (export "set_rw_mem_0") (param $index i32) (param $val i32) 
    (i32.store $rw_mem_0 (local.get $index) (local.get $val))
  )

  (func (export "get_ro_mem_0") (param $index i32) (result i32)
    (i32.load $ro_mem_0 (local.get $index))
  )

  ;; Grows rw_table_0 by $size and initializes new values to $init_val. 
  ;; Returns the old length.
  (func (export "grow_rw_table_0") (param $size i32) (param $init_val externref ) (result i32)
    (local.get $init_val)
    (local.get $size)
    (table.grow $rw_table_0)
  )

  ;; Grows rw_table_1 by $size and initializes new values to $init_val. 
  ;; Returns the old length or -1 for failure.
  (func (export "grow_rw_table_1") (param $size i32) (param $init_val externref ) (result i32)
    (local.get $init_val)
    (local.get $size)
    (table.grow $rw_table_1)
  )

  ;; Grows rw_mem_0 by $size and returns the old length or -1 for failure.
  (func (export "grow_rw_mem_0") (param $size i32) (result i32)
    (local.get $size)
    (memory.grow $rw_mem_0)
  )

  (func (export "set_rw_table_1") (param $index i32) (param $val externref) 
    (table.set $rw_table_1 (local.get $index) (local.get $val))
  )

  (func (export "get_ro_table_0") (param $index i32) (result externref)
    (table.get $ro_table_0 (local.get $index))
  )
)