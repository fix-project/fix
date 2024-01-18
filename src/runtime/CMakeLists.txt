file (GLOB LIB_SOURCES evaluator.cc executor.cc)

add_custom_command(
  OUTPUT "wasm-rt-content.h"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-rt/wasm-rt.h
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/wasm_rt_content.py
          ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-rt/wasm-rt.h
          -o wasm-rt-content.h
)

add_library (runtime STATIC ${LIB_SOURCES} wasm-rt-content.h)
target_include_directories (runtime PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(runtime PUBLIC storage handle component util glog absl::flat_hash_map PRIVATE wasmrt)
