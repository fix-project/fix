cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0115 NEW)

include(FetchContent)

# defines functions link_libc, link_storage, link_wasi_stubs
include(../util/cmake_helper_functions.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BASE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -mreference-types -I ~/wasm-toolchain/wasix/include -fno-exceptions")
set(CMAKE_CXX_COMPILER $ENV{HOME}/wasm-toolchain/sysroot/bin/clang++)
set(CMAKE_CXX_COMPILER_TARGET "wasm32")


add_link_options(-nostdlib -Wl,--no-entry -Wl,--allow-undefined )

add_library(example_obj OBJECT "example.cc")

add_custom_command(
  OUTPUT "asyncify_storage.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/asyncify_storage.wat
  COMMAND ${CMAKE_BINARY_DIR}/../../../_deps/wabt-build/wat2wasm
          --enable-multi-memory
	        --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/asyncify_storage.wat
          -o asyncify_storage.wasm
)

link_libc("${CMAKE_BINARY_DIR}/asyncify/CMakeFiles/example_obj.dir/example.cc.o" example_obj example.wasm) # outputs example.wasm
link_wasm(. asyncify_storage.wasm example.wasm) # outputs linked_with_wasm_0.wasm 
link_another_wasm(../util fixpoint_storage.wasm 1) # outputs linked_with_wasm_1.wasm
link_wasi_stubs(../util wasi_snapshot_preview1.wasm "example_with_wrapper.wasm" 2) # outputs example_with_wrapper.wasm

add_custom_command(
    OUTPUT "example_asyncify.wasm"
    DEPENDS $ENV{HOME}/binaryen/bin/wasm-opt
    example_with_wrapper.wasm 
    COMMAND $ENV{HOME}/binaryen/bin/wasm-opt # TODO(Katie) use correct binaryen
    --asyncify
    --enable-reference-types
    --enable-bulk-memory
    --enable-multi-memories
    --pass-arg=asyncify-ignore-imports
    -Os
    example_with_wrapper.wasm 
    -o example_asyncify.wasm
)

add_custom_target(
        asyncify-test ALL
        DEPENDS example_asyncify.wasm
        )