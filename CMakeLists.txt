cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0115 NEW)

project(fixpoint)

find_package(PkgConfig)
find_package(Boost 1.74.0 REQUIRED COMPONENTS headers)

pkg_search_module(CRYPT REQUIRED libcrypto++ libcryptopp)
include_directories(${CRYPT_INCLUDE_DIRS})
add_compile_options(${CRYPT_CFLAGS_OTHER})

include(FetchContent)
FetchContent_Declare(
  absl
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp
  GIT_TAG        20220623.0
  )
set(ABSL_PROPAGATE_CXX_STD on)
FetchContent_MakeAvailable(absl)

include(ExternalProject)
ExternalProject_Add(applications
  SOURCE_DIR "${PROJECT_SOURCE_DIR}/applications"
  INSTALL_COMMAND cmake -E echo "Skipping applications install."
  BUILD_ALWAYS true
  )

file(READ ${PROJECT_SOURCE_DIR}/boot/compile-encode COMPILE-ENCODE)
message(COMPILE-ENCODE="${COMPILE-ENCODE}")
add_compile_definitions(COMPILE_ENCODE="${COMPILE-ENCODE}")
file(READ ${PROJECT_SOURCE_DIR}/boot/compile-elf COMPILE-ELF)
message(COMPILE-ELF="${COMPILE-ELF}")
add_compile_definitions(COMPILE=\"${COMPILE-ELF}\")

include(etc/build_type.cmake)
include(etc/cflags.cmake)
include(etc/clang_format.cmake)
include(etc/tests.cmake)

include_directories(SYSTEM "${CMAKE_BINARY_DIR}/_deps/absl-src")
include_directories(SYSTEM "${CMAKE_BINARY_DIR}/_deps/absl-build")

file(REAL_PATH .fix FIX_DIR BASE_DIRECTORY ${PROJECT_SOURCE_DIR} EXPAND_TILDE)

include_directories("${PROJECT_SOURCE_DIR}/src/util")
include_directories("${PROJECT_SOURCE_DIR}/src/runtime")
include_directories("${PROJECT_SOURCE_DIR}/src/component")
include_directories("${PROJECT_SOURCE_DIR}/src/wasm-rt")
include_directories("${PROJECT_SOURCE_DIR}/src/storage")

add_subdirectory("${PROJECT_SOURCE_DIR}/src/util")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/runtime")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/component")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/wasm-rt")

set(FIXPOINT_LIBS runtime component util)
set(FIXPOINT_LIBS_TIMING runtime-timing component-timing util-timing)
set(FIXPOINT_LIBS_TIMING_2 runtime-timing-2 component-timing-2 util-timing-2)
#add_subdirectory("${PROJECT_SOURCE_DIR}/src/storage") # only headers

add_subdirectory("${PROJECT_SOURCE_DIR}/src/tester")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/scripts")

add_subdirectory("${PROJECT_SOURCE_DIR}/testing/wasm-examples")
add_subdirectory("${PROJECT_SOURCE_DIR}/testing/benchmark")

# Doxygen documentation generation
find_package(Doxygen)
if (${DOXYGEN_FOUND})
    set(DOXYGEN_EXCLUDE_PATTERNS "*/_deps/*")
    set(DOXYGEN_DOCS_TARGET_NAME "doxygen-docs")
    set(DOXYGEN_CREATE_SUBDIRS "YES")
    set(DOXYGEN_BUILTIN_STL_SUPPORT "YES")
    set(DOXYGEN_EXTRACT_ALL "YES")
    set(DOXYGEN_EXTRACT_PRIVATE "YES")
    set(DOXYGEN_EXTRACT_PRIV_VIRTUAL "YES")
    set(DOXYGEN_EXTRACT_PACKAGE "YES")
    set(DOXYGEN_EXTRACT_STATIC "YES")
    set(DOXYGEN_EXTRACT_LOCAL_CLASSES "YES")
    set(DOXYGEN_EXTRACT_ANON_NSPACES "YES")
    set(DOXYGEN_SOURCE_BROWSER "YES")
    set(DOXYGEN_STRIP_CODE_COMMENTS "NO")
    set(DOXYGEN_REFERENCED_BY_RELATION "YES")
    set(DOXYGEN_REFERENCES_RELATION "YES")
    set(DOXYGEN_HTML_DYNAMIC_SECTIONS "YES")
    #set(DOXYGEN_DISABLE_INDEX "YES")
    set(DOXYGEN_GENERATE_TREEVIEW "YES")
    #set(DOXYGEN_FULL_SIDEBAR "YES")
    set(DOXYGEN_TEMPLATE_RELATIONS "YES")
    set(DOXYGEN_CALL_GRAPH "YES")
    set(DOXYGEN_CALLER_GRAPH "YES")
    set(DOXYGEN_DOT_IMAGE_FORMAT "svg")
    doxygen_add_docs(${DOXYGEN_DOCS_TARGET_NAME})
    message("Doxygen target \"${DOXYGEN_DOCS_TARGET_NAME}\" added")
endif()
