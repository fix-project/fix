cmake_minimum_required ( VERSION 2.8.5 )
project ( fixpoint )


find_package ( PkgConfig )

pkg_check_modules ( CRYPT REQUIRED libcrypto++ )
include_directories ( ${CRYPT_INCLUDE_DIRS} )
add_compile_options ( ${CRYPT_CFLAGS_OTHER} )

set(BUILD_TESTS off)
set(BUILD_TOOLS off)
set(BUILD_LIBWASM off)
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/wabt")

include_directories("${PROJECT_SOURCE_DIR}/src/wabt")

include ( etc/build_type.cmake )
#include ( etc/cflags.cmake )
include ( etc/cflags_temp.cmake )
include ( etc/clang_format.cmake )

include_directories ("${PROJECT_SOURCE_DIR}/src/util")
#include_directories ("${PROJECT_SOURCE_DIR}/src/runtime")
#include_directories ("${PROJECT_SOURCE_DIR}/src/component")
include_directories ("${PROJECT_SOURCE_DIR}/src/compiler")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src/util")
#add_subdirectory ("${PROJECT_SOURCE_DIR}/src/runtime")
#add_subdirectory ("${PROJECT_SOURCE_DIR}/src/component")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/compiler")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/tester")
