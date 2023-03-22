(module
  (import "asyncify" "stop_unwind" (func $stop_unwind))
  (import "asyncify" "start_rewind" (func $start_rewind (param i32)))
  (import "example" "apply" (func $apply (param externref) (result externref)))
  (import "example" "asyncify_memory_offset" (func $asyncify_mem_offset (result i32)))
  
  ;; easy way to convert a c++ pointer into the corresponding wasm memory location
  (func (export "get_memory_offset") (param $offset i32) (result i32)
    local.get $offset
  )

  ;; Defined here and not in example.cc to prevent inlining
  ;; Asyncify fails if anything in this function gets inlined.
  (func (export "_fixpoint_apply") (param $encode externref) (result externref)
    (local.get $encode )
    (call $apply)
    drop
    (call $stop_unwind)
    (call $asyncify_mem_offset)
    (call $start_rewind)
    (local.get $encode)
    (call $apply)
  )
)