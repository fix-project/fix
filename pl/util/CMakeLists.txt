
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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -mreference-types -I ~/wasm-toolchain/wasix/include -fno-exceptions")
set(CMAKE_CXX_COMPILER $ENV{HOME}/wasm-toolchain/sysroot/bin/clang++)

add_link_options( -Wl,--no-entry -Wl,--allow-undefined, -Wl, -Werror, -Wunused-parameter, -fno-exceptions)


add_custom_command(
  OUTPUT "wasi_snapshot_preview1.wasm"
  COMMAND ${CMAKE_BINARY_DIR}/../../../_deps/wabt-build/wat2wasm
          --enable-multi-memory
	        --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/wasi_snapshot_preview1.wat
          -o wasi_snapshot_preview1.wasm
)

add_custom_command(
  OUTPUT "fixpoint_storage.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fixpoint_storage.wat
  COMMAND ${CMAKE_BINARY_DIR}/../../../_deps/wabt-build/wat2wasm
          --enable-multi-memory
	        --enable-exceptions
          ${CMAKE_CURRENT_SOURCE_DIR}/fixpoint_storage.wat
          -o fixpoint_storage.wasm
)

add_custom_target(
              pl-storage ALL
              DEPENDS 
                fixpoint_storage.wasm
                wasi_snapshot_preview1.wasm
              )