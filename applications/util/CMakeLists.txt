
cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0115 NEW)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BASE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Weverything -Werror -Wno-reserved-macro-identifier -Wno-reserved-identifier -Wno-unreachable-code -Wno-unused-command-line-argument -Wno-unused-but-set-variable -Wno-c++98-compat -std=c17 -Os -target wasm32 -mreference-types")

set(CMAKE_C_COMPILER $ENV{HOME}/wasm-toolchain/sysroot/bin/clang)
set(CMAKE_C_COMPILER_TARGET "wasm32")

add_custom_command(
  OUTPUT "wasi_snapshot_preview1.wasm"
  COMMAND $ENV{HOME}/wasm-toolchain/wabt/build/wat2wasm
          --enable-multi-memory
	        --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/wasi_snapshot_preview1.wat
          -o wasi_snapshot_preview1.wasm
)

add_custom_command(
  OUTPUT "fixpoint_storage.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fixpoint_storage.wat
  COMMAND $ENV{HOME}/wasm-toolchain/wabt/build/wat2wasm
          --enable-multi-memory
	        --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/fixpoint_storage.wat
          -o fixpoint_storage.wasm
)

add_library(fixpoint_util_obj OBJECT "fixpoint_util.c")

add_custom_target(
              applications-util ALL
              DEPENDS 
                fixpoint_storage.wasm
                wasi_snapshot_preview1.wasm
                fixpoint_util_obj
              )
