;; This module provides stubs for stdio functions that are needed for wasm-toolchain/sysroot/lib/wasm32-wasi/libc++abi.a


(module
  (func (export "fd_close") (param i32) (result i32)
    unreachable
  )

  (func (export "fd_write") (param i32 i32 i32 i32) (result i32)
    unreachable
  )

  (func (export "fd_seek") (param i32 i64 i32 i32) (result i32)
    unreachable
  )

)