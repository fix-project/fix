add_library(handle-coupon INTERFACE)
target_include_directories(handle-coupon INTERFACE .)
target_include_directories(handle-coupon INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(generate-handle-coupon generate.c)
target_link_libraries(generate-handle-coupon m)

add_custom_command(
  OUTPUT handle.hh
  COMMAND "${CMAKE_CURRENT_BINARY_DIR}/generate-handle-coupon" > "${CMAKE_CURRENT_BINARY_DIR}/handle.hh"
  DEPENDS generate-handle-coupon
)
add_custom_target(handle_hh_coupon DEPENDS handle.hh)
add_dependencies(handle-coupon handle_hh_coupon)
