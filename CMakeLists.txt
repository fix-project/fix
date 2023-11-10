cmake_minimum_required(VERSION 3.16)
project(fixpoint)
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig)
find_package(Boost 1.74.0 REQUIRED COMPONENTS headers)
pkg_search_module(GLOG REQUIRED libglog IMPORTED_TARGET glog)

include(etc/sanitizers.cmake)
add_compile_options(-march=znver2) # for AVX2

set(ABSL_PROPAGATE_CXX_STD on)
add_subdirectory(${PROJECT_SOURCE_DIR}/third-party/abseil-cpp)
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/third-party/abseil-cpp")

include(ExternalProject)
ExternalProject_Add(applications
  SOURCE_DIR "${PROJECT_SOURCE_DIR}/applications"
  INSTALL_COMMAND cmake -E echo "Skipping applications install."
  BUILD_ALWAYS true
  )

include(etc/build_type.cmake)
add_compile_options(-Wall -Wpedantic -Werror -Wextra)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Weffc++>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-unqualified-std-cast-call>)
include(etc/clang_format.cmake)
include(etc/tests.cmake)

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
