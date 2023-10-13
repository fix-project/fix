enable_testing()

add_custom_target (fixpoint-check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^t_"
  COMMENT "Testing..."
)

add_test(NAME t_add WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-add)
add_test(NAME t_fib WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-fib)
add_test(NAME t_trap WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test-trap.sh)
add_test(NAME t_map WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-map)

add_test(NAME t_add_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-add-flatware)
add_test(NAME t_open_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-open-flatware)
add_test(NAME t_return_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-return-flatware)
add_test(NAME t_helloworld_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-helloworld-flatware)

add_test(NAME t_storage WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-storage)
