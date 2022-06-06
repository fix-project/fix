file (GLOB LIB_SOURCES "*.cc")

add_custom_command(
  OUTPUT "wasm-rt-content.h"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-rt/wasm-rt.h
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../compiler/wasm_rt_content.py
          ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-rt/wasm-rt.h
          -o wasm-rt-content.h
)

add_library (runtime STATIC ${LIB_SOURCES} wasm-rt-content.h)
add_library (runtime-timing STATIC ${LIB_SOURCES} wasm-rt-content.h)
target_compile_definitions(runtime-timing PUBLIC TIME_FIXPOINT=1)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
