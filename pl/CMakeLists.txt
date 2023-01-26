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

add_library(test_obj OBJECT "test.cc")
# add_library(map_obj OBJECT "map.cc")

add_custom_command(
  OUTPUT  "test.wasm"
  DEPENDS test_obj
          test.cc
  COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/wasm-ld
          ${CMAKE_BINARY_DIR}/CMakeFiles/test_obj.dir/test.cc.o
          $ENV{HOME}/wasm-toolchain/sysroot/lib/wasm32-wasi/libc++.a
          -o test.wasm
          --no-entry
          --allow-undefined
)

add_custom_command(
  OUTPUT "vector.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vector.wat
  COMMAND ${CMAKE_BINARY_DIR}/../../../_deps/wabt-build/wat2wasm
          --enable-multi-memory
	        --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/vector.wat
          -o vector.wasm
)

add_custom_command(
    OUTPUT "test-memory.wasm"
    DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
    test.wasm
    vector.wasm
    COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
    --enable-multi-memory
    --enable-exceptions
    test.wasm
    vector.wasm
    -o test-memory.wasm
    )

add_custom_target(
        pl-test ALL
        DEPENDS test.wasm
                vector.wasm
                test-memory.wasm
      )
  