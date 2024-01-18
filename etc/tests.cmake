enable_testing()

add_custom_target (fixpoint-check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^t_" -E "t_self_host"
  COMMENT "Testing..."
)
add_custom_target (all-check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^t_" COMMENT "Testing...")

add_test(NAME t_add WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-add)
add_test(NAME t_fib WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-fib)
add_test(NAME t_trap WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test-trap.sh)
add_test(NAME t_map WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-map)
add_test(NAME t_curry WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-curry)
add_test(NAME t_distributed WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-distributed)

add_test(NAME t_add_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-add-flatware)
add_test(NAME t_open_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-open-flatware)
add_test(NAME t_return_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-return-flatware)
add_test(NAME t_helloworld_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-helloworld-flatware)

add_test(NAME t_storage WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-storage)

add_test(NAME t_self_host WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-self-host)
