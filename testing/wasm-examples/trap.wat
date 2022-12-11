(import "fixpoint" "create_blob_i32" (func (param i32) (result externref)))
(memory 1)
(func (export "_fixpoint_apply") (param externref) (result externref)
      i32.const 1
      i32.const 0
      i32.div_u
      drop
      local.get 0)
