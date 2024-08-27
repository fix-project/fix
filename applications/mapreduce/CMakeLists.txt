# defines link_libc
include(../util/cmake_helper_functions.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -mreference-types -I ~/wasm-toolchain/wasix/include -fno-exceptions")
set(CMAKE_CXX_COMPILER $ENV{HOME}/wasm-toolchain/sysroot/bin/clang++)

add_link_options( -Wl,--no-entry -Wl,--allow-undefined, -Wl, -Werror, -Wunused-parameter, -fno-exceptions)

add_custom_command(
  OUTPUT "support.wasm"
  DEPENDS support.wat
  COMMAND $ENV{HOME}/wasm-toolchain/wabt/build/wat2wasm
          --enable-multi-memory
          --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/support.wat
          -o support.wasm
)

add_library(mapreduce_obj OBJECT "mapreduce.cc")
target_link_libraries(mapreduce_obj PUBLIC fixpoint_storage)

link_libc("${CMAKE_BINARY_DIR}/mapreduce/CMakeFiles/mapreduce_obj.dir/mapreduce.cc.o" mapreduce_obj mapreduce_unlinked.wasm) # outputs curry_unlinked.wasm

# link fixpoint_storage, which defines wat tables and memories
add_custom_command(
        OUTPUT "mapreduce.wasm"
        DEPENDS mapreduce_unlinked.wasm
                support.wasm 
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        mapreduce_unlinked.wasm
        support.wasm
        -m wasi_command
        -n support
        -o "mapreduce.wasm"
)

add_custom_target(
        mapreduce ALL
        DEPENDS mapreduce.wasm
        )
