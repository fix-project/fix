file (GLOB LIB_SOURCES "*.cc")

execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../../etc/clang-include-path"
  OUTPUT_VARIABLE FIXPOINT_C_INCLUDE_PATH
  ECHO_OUTPUT_VARIABLE
  COMMAND_ECHO STDOUT
  )

configure_file(include-path.hh.in include-path.hh)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library (compiler STATIC ${LIB_SOURCES} include-path.hh)
