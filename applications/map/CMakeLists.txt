
# defines link_libc
include(../util/cmake_helper_functions.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -mreference-types -I ~/wasm-toolchain/wasix/include -fno-exceptions")
set(CMAKE_CXX_COMPILER $ENV{HOME}/wasm-toolchain/sysroot/bin/clang++)

add_link_options( -Wl,--no-entry -Wl,--allow-undefined, -Wl, -Werror, -Wunused-parameter, -fno-exceptions)

add_library(add_2_obj OBJECT "add_2.cc")

link_libc("${CMAKE_BINARY_DIR}/map/CMakeFiles/add_2_obj.dir/add_2.cc.o" add_2_obj add_2.wasm) # outputs add_2.wasm

# link fixpoint_storage, which defines wat tables and memories
add_custom_command(
        OUTPUT "linked_with_storage.wasm"
        DEPENDS add_2.wasm
                applications-util
                ../util/fixpoint_storage.wasm
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        add_2.wasm
        ../util/fixpoint_storage.wasm
        -n fixpoint_storage
        -o "linked_with_storage.wasm"
)

# link wasi_snapshot_preview1, which defines stubs for fd_write/seek/close
add_custom_command(
        OUTPUT "add_2_map.wasm"
        DEPENDS linked_with_storage.wasm
                applications-util
                ../util/wasi_snapshot_preview1.wasm
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        linked_with_storage.wasm
        ../util/wasi_snapshot_preview1.wasm
        -n wasi_snapshot_preview1
        -o "add_2_map.wasm"
)

add_custom_target(
        map-test ALL
        DEPENDS add_2_map.wasm
        )
