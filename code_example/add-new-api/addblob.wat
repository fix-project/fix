(module
  (type (;0;) (func (param i32 i32)))
  (type (;1;) (func (param i32 i32 i32)))
  (type (;2;) (func (param i32 i32) (result i32)))
  (type (;3;) (func))
  (import "env" "attach_input" (func $attach_input (type 0)))
  (import "env" "attach_output" (func $attach_output (type 1)))
  (import "env" "get_int" (func $get_int (type 2)))
  (import "env" "store_int" (func $store_int (type 0)))
  (func $_start (type 3)
    i32.const 0
    i32.const 0
    call $attach_input
    i32.const 1
    i32.const 1
    call $attach_input
    i32.const 0
    i32.const 0
    i32.const 0
    call $attach_output
    i32.const 0
    i32.const 0
    i32.const 0
    call $get_int
    i32.const 1
    i32.const 0
    call $get_int
    i32.add
    call $store_int)
  (table (;0;) 1 1 funcref)
  (memory (;0;) 2)
  (global (;0;) (mut i32) (i32.const 66576))
  (export "memory" (memory 0))
  (export "_start" (func $_start)))
