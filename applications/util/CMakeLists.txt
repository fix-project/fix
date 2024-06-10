
cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0115 NEW)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BASE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Weverything -Werror -Wno-reserved-macro-identifier -Wno-reserved-identifier -Wno-unreachable-code -Wno-unused-command-line-argument -Wno-unused-but-set-variable -Wno-c++98-compat -Wno-pre-c11-compat -Wno-unknown-warning-option -std=c17 -Os -mreference-types")

set(CMAKE_C_COMPILER $ENV{HOME}/wasm-toolchain/sysroot/bin/clang)
set(CMAKE_C_COMPILER_TARGET "wasm32-wasi")

set_source_files_properties("fixpoint_storage.h" "fixpoint_storage.wat" PROPERTIES GENERATED TRUE)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(
        OUTPUT fixpoint_storage.h fixpoint_storage.wat
        DEPENDS generate_fixpoint_storage
        DEPENDS_EXPLICIT_ONLY
)

add_custom_target(
        generate_fixpoint_storage
        COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/generate.py
        COMMENT "Generating fixpoint_storage"
)

add_library(fixpoint_storage INTERFACE)
target_include_directories(fixpoint_storage INTERFACE .)
target_include_directories(fixpoint_storage INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(fixpoint_storage generate_fixpoint_storage)

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
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fixpoint_storage.wat
  COMMAND $ENV{HOME}/wasm-toolchain/wabt/build/wat2wasm
          --enable-multi-memory
	        --enable-exceptions
          ${CMAKE_CURRENT_BINARY_DIR}/fixpoint_storage.wat
          -o fixpoint_storage.wasm
)

add_library(fixpoint_util_obj OBJECT "fixpoint_util.c")
add_dependencies(fixpoint_util_obj generate_fixpoint_storage)
target_include_directories(fixpoint_util_obj PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(
              applications-util ALL
              DEPENDS 
                fixpoint_storage.wasm
                wasi_snapshot_preview1.wasm
                fixpoint_util_obj
              )
