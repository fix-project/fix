# defines link_libc
include(../util/cmake_helper_functions.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -mreference-types -I ~/wasm-toolchain/wasix/include -fno-exceptions")
set(CMAKE_C_COMPILER $ENV{HOME}/wasm-toolchain/sysroot/bin/clang)

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

add_library(bptree-get-obj OBJECT "bptree-get.c")
target_link_libraries(bptree-get-obj PUBLIC fixpoint_storage)

link_libc("${CMAKE_BINARY_DIR}/bptree-get/CMakeFiles/bptree-get-obj.dir/bptree-get.c.o" bptree-get-obj bptree-get-unlinked.wasm)

# link fixpoint_storage, which defines wat tables and memories
add_custom_command(
        OUTPUT "bptree-get.wasm"
        DEPENDS bptree-get-unlinked.wasm
                support.wasm
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        bptree-get-unlinked.wasm
        support.wasm
        -m wasi_command
        -n support
        -o "bptree-get.wasm"
)

add_custom_target(
        bptree-get ALL
        DEPENDS bptree-get.wasm
        )
