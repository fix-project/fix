cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0115 NEW)

project(pl)

include(FetchContent)
FetchContent_Declare(
  wasm-tools
  GIT_REPOSITORY https://github.com/fix-project/wasm-tools
  GIT_TAG        main
  BUILD_ALWAYS   true
)
FetchContent_MakeAvailable(wasm-tools)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -nostdlib -mreference-types -I ~/wasm-toolchain/wasix/include")
set(CMAKE_CXX_COMPILER $ENV{HOME}/wasm-toolchain/sysroot/bin/clang++)

add_link_options( -nostdlib -Wl,--no-entry -Wl,--allow-undefined, -Wl, -Werror, -Wunused-parameter)

add_library(add_2_obj OBJECT "add_2.cc")

add_custom_command(
  OUTPUT  "add_2.wasm"
  DEPENDS add_2_obj
          add_2.cc
  COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/wasm-ld
          ${CMAKE_BINARY_DIR}/CMakeFiles/add_2_obj.dir/add_2.cc.o
          $ENV{HOME}/wasm-toolchain/sysroot/lib/wasm32-wasi/libc++.a
          -o add_2.wasm
          --no-entry
          --allow-undefined
)

add_custom_command(
  OUTPUT "map_memory.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/map_memory.wat
  COMMAND ${CMAKE_BINARY_DIR}/../../../_deps/wabt-build/wat2wasm
          --enable-multi-memory
	        --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/map_memory.wat
          -o map_memory.wasm
)

add_custom_command(
    OUTPUT "add_2_map.wasm"
    DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
    add_2.wasm
    map_memory.wasm
    COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
    --enable-multi-memory
    --enable-exceptions
    add_2.wasm
    map_memory.wasm
    -o add_2_map.wasm
    )

add_custom_target(
        pl-test ALL
        DEPENDS add_2.wasm
                map_memory.wasm
                add_2_map.wasm
      )
  