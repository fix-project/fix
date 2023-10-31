cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0115 NEW)

project(fixpoint)

find_package(PkgConfig)
find_package(Boost 1.74.0 REQUIRED COMPONENTS headers)
pkg_search_module(GLOG REQUIRED libglog IMPORTED_TARGET glog)

include(FetchContent)
FetchContent_Declare(
  absl
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp
  GIT_TAG        20230802.1
  )
set(ABSL_PROPAGATE_CXX_STD on)
FetchContent_MakeAvailable(absl)

include(ExternalProject)
ExternalProject_Add(applications
  SOURCE_DIR "${PROJECT_SOURCE_DIR}/applications"
  INSTALL_COMMAND cmake -E echo "Skipping applications install."
  BUILD_ALWAYS true
  )

include(etc/build_type.cmake)
include(etc/cflags.cmake)
include(etc/clang_format.cmake)
include(etc/tests.cmake)

include_directories(SYSTEM "${CMAKE_BINARY_DIR}/_deps/absl-src")
include_directories(SYSTEM "${CMAKE_BINARY_DIR}/_deps/absl-build")

include_directories("${PROJECT_SOURCE_DIR}/src/util")
include_directories("${PROJECT_SOURCE_DIR}/src/http")
include_directories("${PROJECT_SOURCE_DIR}/src/runtime")
include_directories("${PROJECT_SOURCE_DIR}/src/component")
include_directories("${PROJECT_SOURCE_DIR}/src/wasm-rt")
include_directories("${PROJECT_SOURCE_DIR}/src/storage")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/third-party")

add_subdirectory("${PROJECT_SOURCE_DIR}/src/util")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/http")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/runtime")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/component")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/wasm-rt")

#add_subdirectory("${PROJECT_SOURCE_DIR}/src/storage") # only headers

add_subdirectory("${PROJECT_SOURCE_DIR}/src/tester")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/tests")

add_subdirectory("${PROJECT_SOURCE_DIR}/testing/wasm-examples")
add_subdirectory("${PROJECT_SOURCE_DIR}/testing/benchmark")

# Doxygen documentation generation
add_subdirectory(docs)
