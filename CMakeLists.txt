cmake_minimum_required ( VERSION 3.18 )
project ( flatware )

find_package ( PkgConfig )


include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
  wabt
  GIT_REPOSITORY https://github.com/fixpointOS/wabt
  GIT_TAG        fixpoint
  )
set(BUILD_TESTS off)
set(BUILD_LIBWASM off)
FetchContent_MakeAvailable(wabt)

include(FetchContent)
set(FETCHCONTENT_QUITE OFF)
FetchContent_Declare(
  wasm-tools
  GIT_REPOSITORY https://github.com/fixpointOS/wasm-tools.git
  GIT_TAG main
  )
FetchContent_MakeAvailable(wasm-tools)
  
include_directories (SYSTEM "${CMAKE_BINARY_DIR}/_deps/wabt-src")
include_directories (SYSTEM "${CMAKE_BINARY_DIR}/_deps/wabt-build")

include_directories (SYSTEM "${CMAKE_BINARY_DIR}/_deps/wasm-tools-src")
include_directories (SYSTEM "${CMAKE_BINARY_DIR}/_deps/wasm-tools-build")

include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/tests.cmake )

include_directories ("${PROJECT_SOURCE_DIR}/src")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src")
