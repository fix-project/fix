cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0115 NEW)

project(flatware)

include(FetchContent)
FetchContent_Declare(
  wasm-tools
  GIT_REPOSITORY https://github.com/fixpointOS/wasm-tools
  GIT_TAG        main
  BUILD_ALWAYS   true
)
FetchContent_MakeAvailable(wasm-tools)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BASE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Weverything -Werror -Wno-reserved-macro-identifier -Wno-reserved-identifier -Wno-unused-command-line-argument -Wno-c++98-compat -std=c17 -Os -target wasm32 -mreference-types")

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET "wasm32")

add_link_options(-nostdlib -Wl,--no-entry -Wl,--allow-undefined)
add_executable(c-flatware.wasm "c-flatware.c")

add_custom_command(
  OUTPUT "asm-flatware.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/asm-flatware.wat
  COMMAND ${CMAKE_BINARY_DIR}/../../../_deps/wabt-build/wat2wasm
          --enable-multi-memory
	  --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/asm-flatware.wat
          -o asm-flatware.wasm
)

add_custom_command(
  OUTPUT "flatware_with_exported_memory.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          asm-flatware.wasm
          c-flatware.wasm
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
          --enable-multi-memory
	  --enable-exceptions
          c-flatware.wasm
          asm-flatware.wasm
          -o flatware_with_exported_memory.wasm
)
set_source_files_properties(asm-flatware.wasm PROPERTIES GENERATED TRUE)

add_custom_command(
  OUTPUT "wasi_snapshot_preview1.wasm"
  DEPENDS flatware_with_exported_memory.wasm
          ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/export-audit/export-audit
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/export-audit/export-audit
          --enable-multi-memory
          --enable-exceptions
          -r memory
          flatware_with_exported_memory.wasm
          wasi_snapshot_preview1.wasm
)

add_custom_target(
  wasi-snapshot ALL
  DEPENDS wasi_snapshot_preview1.wasm
)

add_subdirectory ("examples")
