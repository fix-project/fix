(module
  (type (;0;) (func (param i32) (result i32)))
  (type (;1;) (func (param i32 i32 i32) (result i32)))
  (type (;2;) (func (param i32 i32)))
  (import "env" "path_open" (func $path_open (type 0)))
  (import "env" "fd_write" (func $fd_write (type 1)))
  (func $_start (type 2) (param i32 i32)
    i32.const 0
    local.get 1
    local.get 0
    i32.add
    i32.store
    i32.const 1024
    call $path_open
    i32.const 0
    i32.load
    i32.const 4
    call $fd_write
    drop)
  (table (;0;) 1 1 funcref)
  (memory (;0;) 2)
  (global (;0;) (mut i32) (i32.const 66576))
  (export "memory" (memory 0))
  (export "_start" (func $_start))
  (data (;0;) (i32.const 1024) "output\00"))
