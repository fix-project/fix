add_link_options(-nostdlib -Wl,--no-entry)
add_executable(trivial-demo "proc_exit.c")

add_custom_command(
  OUTPUT "helper.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/memory_helper.wat
          ${CMAKE_BINARY_DIR}/_deps/wabt-build/wat2wasm
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wabt-build/wat2wasm
          --enable-multi-memory
          ${CMAKE_CURRENT_SOURCE_DIR}/memory_helper.wat
          -o helper.wasm
)

add_custom_command(
  OUTPUT "wasi_snapshot_preview1.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          helper.wasm
          trivial-demo
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          --enable-multi-memory
          trivial-demo
          -m flatware 
          helper.wasm
          -o wasi_snapshot_preview1.wasm
)

add_custom_target(
  wasi-snapshot ALL
  DEPENDS wasi_snapshot_preview1.wasm
)

add_subdirectory ("examples")
