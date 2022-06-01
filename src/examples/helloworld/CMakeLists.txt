add_custom_command(
  OUTPUT "sloth.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/helloworld
  COMMAND cp
          ${CMAKE_CURRENT_SOURCE_DIR}/helloworld
          sloth.wasm
)

add_custom_command(
  OUTPUT "wasi_snapshot_preview1.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/src/wasi_snapshot_preview1.wasm
          ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/export-audit/export-audit
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/export-audit/export-audit
          -r memory
          ${CMAKE_BINARY_DIR}/src/wasi_snapshot_preview1.wasm
          wasi_snapshot_preview1.wasm
          --enable-multi-memory
)

add_custom_command(
  OUTPUT "output.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          sloth.wasm
          wasi_snapshot_preview1.wasm
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          --enable-multi-memory
          sloth.wasm
          wasi_snapshot_preview1.wasm
          -n wasi_snapshot_preview1
          -o output.wasm
)

add_custom_target(
  helloworld_output ALL
  DEPENDS output.wasm 
)
add_dependencies(helloworld_output wasi-snapshot)
