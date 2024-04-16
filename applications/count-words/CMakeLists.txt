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

add_library(count_words_obj OBJECT "count_words.cc")
target_link_libraries(count_words_obj PUBLIC fixpoint_storage)

link_libc("${CMAKE_BINARY_DIR}/count-words/CMakeFiles/count_words_obj.dir/count_words.cc.o" count_words_obj count_words_unlinked.wasm)

# link fixpoint_storage, which defines wat tables and memories
add_custom_command(
        OUTPUT "count_words_partial.wasm"
        DEPENDS count_words_unlinked.wasm
                support.wasm
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        count_words_unlinked.wasm
        support.wasm
        -m wasi_command
        -n support
        -o "count_words_partial.wasm"
)
add_custom_command(
        OUTPUT "count_words.wasm"
        DEPENDS count_words_partial.wasm
                applications-util
                ../util/fixpoint_storage.wasm
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        count_words_partial.wasm
        ../util/fixpoint_storage.wasm
        -n fixpoint_storage
        -o "count_words.wasm"
)

add_custom_target(
        count_words ALL
        DEPENDS count_words.wasm
        )

add_library(merge_counts_obj OBJECT "merge_counts.cc")
target_link_libraries(merge_counts_obj PUBLIC fixpoint_storage)

link_libc("${CMAKE_BINARY_DIR}/count-words/CMakeFiles/merge_counts_obj.dir/merge_counts.cc.o" merge_counts_obj merge_counts_unlinked.wasm)

# link fixpoint_storage, which defines wat tables and memories
add_custom_command(
        OUTPUT "merge_counts_partial.wasm"
        DEPENDS merge_counts_unlinked.wasm
                support.wasm
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        merge_counts_unlinked.wasm
        support.wasm
        -m wasi_command
        -n support
        -o "merge_counts_partial.wasm"
)
add_custom_command(
        OUTPUT "merge_counts.wasm"
        DEPENDS merge_counts_partial.wasm
                applications-util
                ../util/fixpoint_storage.wasm
  COMMAND $ENV{HOME}/wasm-toolchain/wasm-tools/build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        merge_counts_partial.wasm
        ../util/fixpoint_storage.wasm
        -n fixpoint_storage
        -o "merge_counts.wasm"
)

add_custom_target(
        merge_counts ALL
        DEPENDS merge_counts.wasm
        )
