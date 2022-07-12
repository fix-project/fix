add_custom_command(
  OUTPUT "return3-fixpoint.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          ${CMAKE_CURRENT_SOURCE_DIR}/return3-wasi.wasm
          ../../c-flatware.c
          ../../asm-flatware.wat
          wasi-snapshot
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          --enable-multi-memory
          --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/return3-wasi.wasm
          ../../wasi_snapshot_preview1.wasm
          -m wasi_command
          -n wasi_snapshot_preview1
          -o return3-fixpoint.wasm
)

add_custom_target(
  return3_fixpoint ALL
  DEPENDS return3-fixpoint.wasm
)
