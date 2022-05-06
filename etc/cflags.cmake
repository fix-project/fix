set (CMAKE_EXPORT_COMPILE_COMMANDS ON)
set (CMAKE_BASE_C_FLAGS "${CMAKE_C_FLAGS}")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Weverything -Werror -std=c17 -target wasm32")

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET "wasm32")
