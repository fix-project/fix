add_custom_command(
  OUTPUT "filesys-wasi.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/filesys.c
  COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/clang
          ${CMAKE_CURRENT_SOURCE_DIR}/filesys.c
          -o filesys-wasi.wasm
)

add_custom_command(
  OUTPUT "filesys-fixpoint.wasm"
  DEPENDS filesys-wasi.wasm
          ../../c-flatware.c
          ../../filesys.c
          ../../asm-flatware.wat
          ../../asm-flatware.h
          wasi-snapshot
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
          --enable-multi-memory
          --enable-exceptions
          filesys-wasi.wasm
          ../../wasi_snapshot_preview1.wasm
          -m wasi_command
          -n wasi_snapshot_preview1
          -o filesys-fixpoint.wasm
)

add_custom_target(
  filesys_fixpoint ALL
  DEPENDS filesys-fixpoint.wasm
)
