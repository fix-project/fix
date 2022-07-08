file (GLOB LIB_SOURCES "*.cc")

add_custom_command(
  OUTPUT "wasm-rt-content.h"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-rt/wasm-rt.h
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/wasm_rt_content.py
          ${CMAKE_CURRENT_SOURCE_DIR}/../wasm-rt/wasm-rt.h
          -o wasm-rt-content.h
)

execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../../etc/clang-include-path"
  OUTPUT_VARIABLE FIXPOINT_C_INCLUDE_PATH
  ECHO_OUTPUT_VARIABLE
  COMMAND_ECHO STDOUT
  )

configure_file(include-path.hh.in include-path.hh)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library (runtime STATIC ${LIB_SOURCES} wasm-rt-content.h include-path.hh)
add_library (runtime-timing STATIC ${LIB_SOURCES} wasm-rt-content.h include-path.hh)
target_compile_definitions(runtime-timing PUBLIC TIME_FIXPOINT=1)
add_library (runtime-timing-2 STATIC ${LIB_SOURCES} wasm-rt-content.h include-path.hh)
target_compile_definitions(runtime-timing-2 PUBLIC TIME_FIXPOINT=2)
