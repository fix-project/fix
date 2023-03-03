add_custom_command(
  OUTPUT "write-file-wasi.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/write-file.c
  COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/clang
          ${CMAKE_CURRENT_SOURCE_DIR}/write-file.c
          -o write-file-wasi.wasm
)

add_custom_command(
  OUTPUT "write-file-fixpoint.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          write-file-wasi.wasm
          ../../c-flatware.c
          ../../filesys.c
          ../../asm-flatware.wat
          ../../asm-flatware.h
          wasi-snapshot 
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          --enable-multi-memory
          --enable-exceptions
          write-file-wasi.wasm
          ../../wasi_snapshot_preview1.wasm
          -m wasi_command
          -n wasi_snapshot_preview1
          -o write-file-fixpoint.wasm
)

add_custom_target(
  write_file_fixpoint ALL
  DEPENDS write-file-fixpoint.wasm
)
