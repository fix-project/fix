(module
  (type (;0;) (func (param i32 i32) (result i32)))
  (type (;1;) (func (param i32 i32)))
  (type (;2;) (func (param i32) (result i32)))
  (type (;3;) (func (param i32)))
  (type (;4;) (func))
  (type (;5;) (func (param i32 i32 i32 i32) (result i32)))
  (type (;6;) (func (result i32)))
  (import "env" "memory" (memory (;0;) 2))
  (import "env" "get_int8" (func $get_int8 (type 0)))
  (import "env" "mem_store" (func $mem_store (type 1)))
  (import "env" "mem_load" (func $mem_load (type 2)))
  (import "env" "mem_store8" (func $mem_store8 (type 1)))
  (import "env" "mem_load8" (func $mem_load8 (type 2)))
  (import "wasi_snapshot_preview1" "proc_exit" (func $__imported_wasi_snapshot_preview1_proc_exit (type 3)))
  (func $_start (type 4)
    (local i32)
    block  ;; label = @1
      call $__original_main
      local.tee 0
      i32.eqz
      br_if 0 (;@1;)
      local.get 0
      call $exit
      unreachable
    end)
  (func $__my_wasi_fd_read (type 5) (param i32 i32 i32 i32) (result i32)
    (local i32 i32 i32)
    block  ;; label = @1
      local.get 2
      i32.eqz
      br_if 0 (;@1;)
      local.get 0
      i32.const 2
      i32.shl
      i32.const 1024
      i32.add
      local.tee 4
      call $mem_load
      local.set 5
      i32.const 0
      local.set 6
      loop  ;; label = @2
        local.get 0
        local.get 5
        call $get_int8
        local.set 5
        local.get 1
        call $mem_load
        local.get 6
        i32.add
        local.get 5
        call $mem_store8
        local.get 4
        local.get 4
        call $mem_load
        i32.const 1
        i32.add
        local.tee 5
        call $mem_store
        local.get 2
        local.get 6
        i32.const 1
        i32.add
        local.tee 6
        i32.ne
        br_if 0 (;@2;)
      end
    end
    local.get 2
    i32.const 65535
    i32.and)
  (func $__original_main (type 6) (result i32)
    i32.const 0
    i32.const 0
    call $mem_store
    i32.const 0
    call $mem_load
    drop
    i32.const 0
    i32.const 0
    call $mem_store8
    i32.const 0
    call $mem_load8
    drop
    i32.const 0)
  (func $__wasi_proc_exit (type 3) (param i32)
    local.get 0
    call $__imported_wasi_snapshot_preview1_proc_exit
    unreachable)
  (func $_Exit (type 3) (param i32)
    local.get 0
    call $__wasi_proc_exit
    unreachable)
  (func $dummy (type 4))
  (func $__wasm_call_dtors (type 4)
    call $dummy
    call $dummy)
  (func $exit (type 3) (param i32)
    call $dummy
    call $dummy
    local.get 0
    call $_Exit
    unreachable)
  (func $_start.command_export (type 4)
    call $_start
    call $__wasm_call_dtors)
  (func $__my_wasi_fd_read.command_export (type 5) (param i32 i32 i32 i32) (result i32)
    local.get 0
    local.get 1
    local.get 2
    local.get 3
    call $__my_wasi_fd_read
    call $__wasm_call_dtors)
  (table (;0;) 1 1 funcref)
  (global $__stack_pointer (mut i32) (i32.const 66592))
  (export "_start" (func $_start.command_export))
  (export "my_wasi_fd_read" (func $__my_wasi_fd_read.command_export))
  (data $.bss (i32.const 1024) "\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00"))
