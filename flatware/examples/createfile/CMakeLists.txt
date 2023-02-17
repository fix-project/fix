add_custom_command(
  OUTPUT "create-new-file-wasi.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/create-new-file.c
  COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/clang
          ${CMAKE_CURRENT_SOURCE_DIR}/create-new-file.c
          -o create-new-file-wasi.wasm
)

add_custom_command(
  OUTPUT "create-new-file-fixpoint.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          create-new-file-wasi.wasm
          ../../c-flatware.c
          ../../filesys.c
          ../../asm-flatware.wat
          ../../asm-flatware.h
          wasi-snapshot 
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          --enable-multi-memory
          --enable-exceptions
          create-new-file-wasi.wasm
          ../../wasi_snapshot_preview1.wasm
          -m wasi_command
          -n wasi_snapshot_preview1
          -o create-new-file-fixpoint.wasm
)

add_custom_target(
  create_new_file_fixpoint ALL
  DEPENDS create-new-file-fixpoint.wasm
)
