add_custom_command(
  OUTPUT "helloworld-fixpoint.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          ${CMAKE_CURRENT_SOURCE_DIR}/helloworld-wasi.wasm
          ../../wasi-snapshot
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          --enable-multi-memory
          --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/helloworld-wasi.wasm
          ../../wasi_snapshot_preview1.wasm
	  -m wasi_command
          -n wasi_snapshot_preview1
          -o helloworld-fixpoint.wasm
)

add_custom_target(
  helloworld_fixpoint ALL
  DEPENDS helloworld-fixpoint.wasm
)
