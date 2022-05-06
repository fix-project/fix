cmake_minimum_required ( VERSION 2.8.5 )
cmake_policy (SET CMP0048 NEW)
project ( fixpoint )

find_package ( PkgConfig )

include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/tests.cmake )

include_directories ("${PROJECT_SOURCE_DIR}/src")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src")
