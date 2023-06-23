
# defines link_libc
include(../util/cmake_helper_functions.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O1 -I ~/wasm-toolchain/wasix/include -mreference-types")
set(CMAKE_C_COMPILER $ENV{HOME}/wasm-toolchain/sysroot/bin/clang)

add_link_options( -Wl,--no-entry -Wl,--allow-undefined, -Wl, -Werror, -Wunused-parameter, -fno-exceptions)

add_library(basic_obj OBJECT "basic.c")

link_libc("${CMAKE_BINARY_DIR}/scheduler/CMakeFiles/basic_obj.dir/basic.c.o" basic_obj basic.wasm.1)

add_custom_command(
        OUTPUT "basic.wasm.2"
        DEPENDS basic.wasm.1
                applications-util
                ../util/fixpoint_storage.wasm
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        basic.wasm.1
        ../util/fixpoint_storage.wasm
        -n fixpoint_storage
        -o "basic.wasm.2"
)

add_custom_command(
        OUTPUT "basic.wasm"
        DEPENDS basic.wasm.2
                applications-util
                ../util/wasi_snapshot_preview1.wasm
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        basic.wasm.2
        ../util/wasi_snapshot_preview1.wasm
        -n wasi_snapshot_preview1
        -m wasi_command
        -o "basic.wasm"
)

add_custom_target(
  scheduler-test ALL
  DEPENDS basic.wasm
)
