file(
  DOWNLOAD
  https://github.com/vmware-labs/webassembly-language-runtimes/releases/download/python%2F3.12.0%2B20231211-040d5a6/python-3.12.0.wasm
  ${CMAKE_CURRENT_BINARY_DIR}/python.wasm
  SHOW_PROGRESS
)

add_custom_command(
  OUTPUT "python-fixpoint.wasm"
  DEPENDS python.wasm
          ../../c-flatware.c
          ../../filesys.c
          ../../asm-flatware.wat
          ../../asm-flatware.h
          wasi-snapshot
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
          --enable-multi-memory
          --enable-exceptions
          python.wasm
          ../../wasi_snapshot_preview1.wasm
          -m wasi_command
          -n wasi_snapshot_preview1
          -o python-fixpoint.wasm
)

add_custom_target(
  python_fixpoint
  DEPENDS python-fixpoint.wasm
)
