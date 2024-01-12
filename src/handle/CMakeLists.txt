add_library(handle INTERFACE)
target_include_directories(handle INTERFACE .)
target_include_directories(handle INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(generate-handle generate.c)
target_link_libraries(generate-handle m)

add_custom_command(
  OUTPUT handle.hh
  COMMAND "${CMAKE_CURRENT_BINARY_DIR}/generate-handle" > "${CMAKE_CURRENT_BINARY_DIR}/handle.hh"
  DEPENDS generate-handle
)
add_custom_target(handle_hh DEPENDS handle.hh)
add_dependencies(handle handle_hh)
