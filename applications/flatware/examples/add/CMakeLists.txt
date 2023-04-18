add_custom_command(
  OUTPUT "add-wasi.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/add_program.c
  COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/clang
          ${CMAKE_CURRENT_SOURCE_DIR}/add_program.c
          -o add-wasi.wasm
)

add_custom_command(
  OUTPUT "add-fixpoint.wasm"
  DEPENDS add-wasi.wasm
          ../../c-flatware.c
          ../../filesys.c
          ../../asm-flatware.wat
          ../../asm-flatware.h
          wasi-snapshot
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
          --enable-multi-memory
          --enable-exceptions
          add-wasi.wasm
          ../../wasi_snapshot_preview1.wasm
          -m wasi_command
          -n wasi_snapshot_preview1
          -o add-fixpoint.wasm
)

add_custom_target(
  add_fixpoint ALL
  DEPENDS add-fixpoint.wasm
)
