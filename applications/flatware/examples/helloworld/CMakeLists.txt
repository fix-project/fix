add_custom_command(
  OUTPUT "helloworld-wasi.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hello.c
  COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/clang
          ${CMAKE_CURRENT_SOURCE_DIR}/hello.c
          -o helloworld-wasi.wasm
)

add_custom_command(
  OUTPUT "helloworld-fixpoint.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          helloworld-wasi.wasm
          ../../c-flatware.c
          ../../filesys.c
          ../../asm-flatware.wat
          ../../asm-flatware.h
          wasi-snapshot
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          --enable-multi-memory
          --enable-exceptions
          helloworld-wasi.wasm
          ../../wasi_snapshot_preview1.wasm
          -m wasi_command
          -n wasi_snapshot_preview1
          -o helloworld-fixpoint.wasm
)

add_custom_target(
  helloworld_fixpoint ALL
  DEPENDS helloworld-fixpoint.wasm
)
