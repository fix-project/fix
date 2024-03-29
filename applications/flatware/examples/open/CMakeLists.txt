add_custom_command(
  OUTPUT "open-wasi.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/open.c
  COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/clang
          ${CMAKE_CURRENT_SOURCE_DIR}/open.c
          -o open-wasi.wasm
)

add_custom_command(
  OUTPUT "open-deep-wasi.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/open-deep.c
  COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/clang
          ${CMAKE_CURRENT_SOURCE_DIR}/open-deep.c
          -o open-deep-wasi.wasm
)

add_custom_command(
  OUTPUT "open-fixpoint.wasm"
  DEPENDS open-wasi.wasm
          ../../c-flatware.c
          ../../filesys.c
          ../../asm-flatware.wat
          ../../asm-flatware.h
          wasi-snapshot
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
          --enable-multi-memory
          --enable-exceptions
          open-wasi.wasm
          ../../wasi_snapshot_preview1.wasm
          -m wasi_command
          -n wasi_snapshot_preview1
          -o open-fixpoint.wasm
)

add_custom_command(
  OUTPUT "open-deep-fixpoint.wasm"
  DEPENDS open-deep-wasi.wasm
          ../../c-flatware.c
          ../../filesys.c
          ../../asm-flatware.wat
          ../../asm-flatware.h
          wasi-snapshot
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
          --enable-multi-memory
          --enable-exceptions
          open-deep-wasi.wasm
          ../../wasi_snapshot_preview1.wasm
          -m wasi_command
          -n wasi_snapshot_preview1
          -o open-deep-fixpoint.wasm
)

add_custom_target(
  open_fixpoint ALL
  DEPENDS open-fixpoint.wasm
)

add_custom_target(
  open_deep_fixpoint ALL
  DEPENDS open-deep-fixpoint.wasm
)
