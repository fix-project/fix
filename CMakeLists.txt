cmake_minimum_required ( VERSION 2.8.5 )
project ( fixpoint )


find_package ( PkgConfig )

pkg_check_modules ( CRYPT REQUIRED libcrypto++ )
include_directories ( ${CRYPT_INCLUDE_DIRS} )
add_compile_options ( ${CRYPT_CFLAGS_OTHER} )

find_package ( Clang REQUIRED )
include_directories ( SYSTEM ${CLANG_INCLUDE_DIRS} )

find_package ( LLVM REQUIRED )
include_directories ( SYSTEM ${LLVM_INCLUDE_DIRS} ) 
add_definitions( ${LLVM_DEFINITIONS} )
llvm_map_components_to_libnames( llvm_libs support )

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
  wabt
  GIT_REPOSITORY https://github.com/WebAssembly/wabt
  GIT_TAG        master
  )
set(BUILD_TESTS off)
set(BUILD_LIBWASM off)
FetchContent_MakeAvailable(wabt)

include ( etc/build_type.cmake )
include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )

include_directories (SYSTEM "${CMAKE_BINARY_DIR}/_deps/wabt-src")
include_directories (SYSTEM "${CMAKE_BINARY_DIR}/_deps/wabt-build")

include_directories ("${PROJECT_SOURCE_DIR}/src/util")
#include_directories ("${PROJECT_SOURCE_DIR}/src/runtime")
#include_directories ("${PROJECT_SOURCE_DIR}/src/component")
include_directories ("${PROJECT_SOURCE_DIR}/src/compiler")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src/util")
#add_subdirectory ("${PROJECT_SOURCE_DIR}/src/runtime")
#add_subdirectory ("${PROJECT_SOURCE_DIR}/src/component")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/compiler")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/tester")
