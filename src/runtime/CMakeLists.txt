file (GLOB LIB_SOURCES "*.cc")
add_library (runtime STATIC ${LIB_SOURCES} wasm-rt-content.h)

add_custom_command(
  OUTPUT "wasm-rt-content.h"
  DEPENDS ${CMAKE_SOURCE_DIR}/src/wasm-rt/wasm-rt.h
  COMMAND ${CMAKE_SOURCE_DIR}/src/compiler/wasm_rt_content.py
          ${CMAKE_SOURCE_DIR}/src/wasm-rt/wasm-rt.h
          -o wasm-rt-content.h
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
