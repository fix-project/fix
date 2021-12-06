(module
  (memory $m1 1)
  (memory $m2 1)

  (func (export "store1") (param i32 i64)
      (memory.size 0)
      drop
      local.get 0
      local.get 1
      i64.store 1)
)
