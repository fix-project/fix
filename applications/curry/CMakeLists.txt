# defines link_libc
include(../util/cmake_helper_functions.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -mreference-types -I ~/wasm-toolchain/wasix/include -fno-exceptions")
set(CMAKE_CXX_COMPILER $ENV{HOME}/wasm-toolchain/sysroot/bin/clang++)

add_link_options( -Wl,--no-entry -Wl,--allow-undefined, -Wl, -Werror, -Wunused-parameter, -fno-exceptions)

add_library(curry_obj OBJECT "curry.cc")

link_libc("${CMAKE_BINARY_DIR}/curry/CMakeFiles/curry_obj.dir/curry.cc.o" curry_obj curry_unlinked.wasm) # outputs curry_unlinked.wasm

# link fixpoint_storage, which defines wat tables and memories
add_custom_command(
        OUTPUT "curry.wasm"
        DEPENDS curry_unlinked.wasm
                applications-util
                ../util/fixpoint_storage.wasm
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        curry_unlinked.wasm
        ../util/fixpoint_storage.wasm
        -n fixpoint_storage
        -o "curry.wasm"
)

add_custom_target(
        curry ALL
        DEPENDS curry.wasm
        )
