
# defines functions link_libc, link_storage, link_wasi_stubs
include(../util/cmake_helper_functions.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -mreference-types -I ~/wasm-toolchain/wasix/include -fno-exceptions")
set(CMAKE_CXX_COMPILER $ENV{HOME}/wasm-toolchain/sysroot/bin/clang++)

add_link_options( -Wl,--no-entry -Wl,--allow-undefined, -Wl, -Werror, -Wunused-parameter, -fno-exceptions)

add_library(add_2_obj OBJECT "add_2.cc")

link_libc("${CMAKE_BINARY_DIR}/map/CMakeFiles/add_2_obj.dir/add_2.cc.o" add_2_obj) # outputs linked_with_libc.wasm
link_storage(../util fixpoint_storage.wasm) # outputs linked_with_stubs.wasm
link_wasi_stubs(../util wasi_snapshot_preview1.wasm "add_2_map.wasm") # outputs add_2_map.wasm


add_custom_target(
        map-test ALL
        DEPENDS add_2_map.wasm
        )