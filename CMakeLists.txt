cmake_minimum_required ( VERSION 3.18 )
project ( flatware )

find_package ( PkgConfig )


include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
  wabt
  GIT_REPOSITORY https://github.com/fixpointOS/wabt
  GIT_TAG        externref_init 
  )
set(BUILD_TESTS off)
set(BUILD_LIBWASM off)
FetchContent_MakeAvailable(wabt)

FetchContent_Declare(
  wasm-tools
  GIT_REPOSITORY https://github.com/fixpointOS/wasm-tools.git
  GIT_TAG main
  )
FetchContent_MakeAvailable(wasm-tools)

FetchContent_Declare(
  fixpoint
  GIT_REPOSITORY https://github.com/fixpointOS/fixpoint
  GIT_TAG master
  )
FetchContent_MakeAvailable(fixpoint)
  
include_directories (SYSTEM "${CMAKE_BINARY_DIR}/_deps/wabt-src")
include_directories (SYSTEM "${CMAKE_BINARY_DIR}/_deps/wabt-build")

include_directories (SYSTEM "${CMAKE_BINARY_DIR}/_deps/wasm-tools-src")
include_directories (SYSTEM "${CMAKE_BINARY_DIR}/_deps/wasm-tools-build")

include_directories (SYSTEM "${CMAKE_BINARY_DIR}/_deps/fixpoint-src")
include_directories (SYSTEM "${CMAKE_BINARY_DIR}/_deps/fixpoint-build")

include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/tests.cmake )

include_directories ("${PROJECT_SOURCE_DIR}/src")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src")
