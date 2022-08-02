add_custom_command(
  OUTPUT "open-wasi.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/open.c
  COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/clang
          ${CMAKE_CURRENT_SOURCE_DIR}/open.c
          -o open-wasi.wasm
)

add_custom_command(
  OUTPUT "open-fixpoint.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          open-wasi.wasm
          ../../c-flatware.c
          ../../filesys.c
          ../../asm-flatware.wat
          ../../asm-flatware.h
          wasi-snapshot 
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          --enable-multi-memory
          --enable-exceptions
          open-wasi.wasm
          ../../wasi_snapshot_preview1.wasm
          -m wasi_command
          -n wasi_snapshot_preview1
          -o open-fixpoint.wasm
)

add_custom_target(
  open_fixpoint ALL
  DEPENDS open-fixpoint.wasm
)
