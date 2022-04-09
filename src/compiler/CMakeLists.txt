file (GLOB LIB_SOURCES "*.cc")

execute_process(COMMAND "${CMAKE_SOURCE_DIR}/etc/clang-include-path"
  OUTPUT_VARIABLE FIXPOINT_C_INCLUDE_PATH
  ECHO_OUTPUT_VARIABLE
  COMMAND_ECHO STDOUT
  )

configure_file(include-path.hh.in include-path.hh)

add_custom_command(
  OUTPUT "wasm-rt-content.h"
  DEPENDS ${CMAKE_SOURCE_DIR}/src/wasm-rt/wasm-rt.h
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/wasm_rt_content.py
          ${CMAKE_SOURCE_DIR}/src/wasm-rt/wasm-rt.h
          -o wasm-rt-content.h
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library (compiler STATIC ${LIB_SOURCES} include-path.hh)
