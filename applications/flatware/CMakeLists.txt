cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0115 NEW)

include(FetchContent)

# defines functions link_libc, link_storage, link_wasi_stubs
include(../util/cmake_helper_functions.cmake)

FetchContent_Declare(
  wasm-tools
  GIT_REPOSITORY https://github.com/fix-project/wasm-tools
  GIT_TAG        main
  BUILD_ALWAYS   true
)
FetchContent_MakeAvailable(wasm-tools)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BASE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Weverything -Werror -Wno-reserved-macro-identifier -Wno-reserved-identifier -Wno-unreachable-code -Wno-unused-command-line-argument -Wno-unused-but-set-variable -Wno-c++98-compat -std=c17 -Os -target wasm32 -mreference-types")

set(CMAKE_C_COMPILER $ENV{HOME}/wasm-toolchain/sysroot/bin/clang)
set(CMAKE_C_COMPILER_TARGET "wasm32")

add_link_options(-nostdlib -Wl,--no-entry -Wl,--allow-undefined)
# add_executable(c-flatware.wasm "c-flatware.c" "filesys.c")

add_library(filesys_obj OBJECT "filesys.c")
add_library(c_flatware_obj OBJECT "c-flatware.c")
add_library(asm_flatware_obj OBJECT "asm-flatware.c")

add_custom_command(
  OUTPUT  "c-flatware.wasm"
  DEPENDS filesys_obj c_flatware_obj asm_flatware_obj util/fixpoint_util_obj filesys.c c-flatware.c asm-flatware.c
  COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/wasm-ld
          ${CMAKE_BINARY_DIR}/flatware/CMakeFiles/filesys_obj.dir/filesys.c.o
          ${CMAKE_BINARY_DIR}/util/CMakeFiles/fixpoint_util_obj.dir/fixpoint_util.c.o
          ${CMAKE_BINARY_DIR}/flatware/CMakeFiles/c_flatware_obj.dir/c-flatware.c.o
          ${CMAKE_BINARY_DIR}/flatware/CMakeFiles/asm_flatware_obj.dir/asm-flatware.c.o
          $ENV{HOME}/wasm-toolchain/sysroot/lib/wasm32-wasi/libc.a
          -o c-flatware.wasm
          --no-entry
          --allow-undefined
)



add_custom_command(
  OUTPUT "asm-flatware-without-fixpoint-storage.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/asm-flatware.wat
  COMMAND ${CMAKE_BINARY_DIR}/../../../_deps/wabt-build/wat2wasm
          --enable-multi-memory
	        --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/asm-flatware.wat
          -o asm-flatware-without-fixpoint-storage.wasm
)
add_custom_command(
  OUTPUT "fixpoint_storage.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        asm-flatware-without-fixpoint-storage.wasm
        ../util/fixpoint_storage.wasm
        COMMAND cp ../util/fixpoint_storage.wasm fixpoint_storage_0.wasm && ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        fixpoint_storage_0.wasm
        asm-flatware-without-fixpoint-storage.wasm
        -o "fixpoint_storage.wasm"
)
set_source_files_properties(fixpoint_storage.wasm PROPERTIES GENERATED TRUE)

add_custom_command(
  OUTPUT "linked_with_storage.wasm"
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        fixpoint_storage.wasm
        c-flatware.wasm
        COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        c-flatware.wasm
        fixpoint_storage.wasm
        -o "linked_with_storage.wasm"
)

add_custom_command(
  OUTPUT "wasi_snapshot_preview1.wasm"
  DEPENDS linked_with_storage.wasm
          ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/export-audit/export-audit
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/export-audit/export-audit
          --enable-multi-memory
          --enable-exceptions
          -r memory
          linked_with_storage.wasm
          wasi_snapshot_preview1.wasm
)

add_custom_target(
  wasi-snapshot ALL
  DEPENDS wasi_snapshot_preview1.wasm
)

add_custom_target(import-check-flatware ALL
  DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/import-check/import-check
          wasi_snapshot_preview1.wasm
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/import-check/import-check
          --enable-multi-memory
          --enable-exceptions
          -e wasi_snapshot_preview1
          wasi_snapshot_preview1.wasm
)

add_subdirectory ("examples")
