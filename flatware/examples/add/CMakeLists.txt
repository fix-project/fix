add_custom_command(
  OUTPUT "add-fixpoint.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          ${CMAKE_CURRENT_SOURCE_DIR}/add-wasi.wasm
          ../../c-flatware.c
          ../../asm-flatware.wat
          wasi-snapshot 
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          --enable-multi-memory
          --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/add-wasi.wasm
          ../../wasi_snapshot_preview1.wasm
          -m wasi_command
          -n wasi_snapshot_preview1
          -o add-fixpoint.wasm
)

add_custom_target(
  add_fixpoint ALL
  DEPENDS add-fixpoint.wasm
)
