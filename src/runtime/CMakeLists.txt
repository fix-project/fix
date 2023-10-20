file (GLOB LIB_SOURCES "*.cc")

add_custom_command(
  OUTPUT "wasm-rt-content.h"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-rt/wasm-rt.h
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/wasm_rt_content.py
          ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-rt/wasm-rt.h
          -o wasm-rt-content.h
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library (runtime STATIC ${LIB_SOURCES} wasm-rt-content.h)
target_link_libraries(runtime PUBLIC component util glog PRIVATE wasmrt absl::flat_hash_map)
