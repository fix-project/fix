enable_testing()

add_custom_target (unit-test COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^u_"
  COMMENT "Unit Tests..."
)
add_custom_target (fixpoint-check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^t_" -E "t_self_host"
  COMMENT "Testing Fix..."
)
add_custom_target (local-fixpoint-check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^l_" -E "l_self_host"
  COMMENT "Testing Fix..."
)
add_custom_target (all-fixpoint-check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^t_" COMMENT "Testing Fix...")
add_custom_target (all-local-fixpoint-check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^l_"
  COMMENT "Testing Fix..."
)

add_custom_target (flatware-check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^f_" -E "f_python_flatware" -E "f_build_python"
  COMMENT "Testing Flatware..."
)
add_custom_target (all-flatware-check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^f_" COMMENT "Testing Flatware...")

add_test(NAME u_handle COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-handle)
add_test(NAME u_hash_table COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-hash-table)
add_test(NAME u_storage COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-storage)
add_test(NAME u_evaluator COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-evaluator)
add_test(NAME u_executor COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-executor)
add_test(NAME u_distributed COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-distributed)
add_test(NAME u_blake3 WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-blake3)
add_test(NAME u_dependency_graph COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-dependency-graph)
add_test(NAME u_pass_scheduler COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-pass-scheduler)
add_test(NAME u_local_scheduler COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-local-scheduler)
add_test(NAME u_relater COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-scheduler-relate)

add_test(NAME t_add COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-add)
add_test(NAME t_fib COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-fib)
add_test(NAME t_bptree COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-bptree-get)
add_test(NAME t_trap COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test-trap.sh)
add_test(NAME t_resource_limits COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test-resource-limits.sh)
add_test(NAME t_map COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-map)
add_test(NAME t_curry COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-curry)
add_test(NAME t_mapreduce COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-mapreduce)
add_test(NAME t_count_words COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-countwords)
add_test(NAME t_self_host WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-self-host)
# add_test(NAME t_api WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test-api.py)

add_test(NAME l_add COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-add -s local)
add_test(NAME l_fib COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-fib -s local)
add_test(NAME l_bptree COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-bptree-get -s local)
add_test(NAME l_trap COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test-trap.sh local)
add_test(NAME l_resource_limits COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/test-resource-limits.sh local)
add_test(NAME l_map COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-map -s local)
add_test(NAME l_curry COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-curry -s local)
add_test(NAME l_mapreduce COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-mapreduce -s local)
add_test(NAME l_count_words COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-countwords -s local)
add_test(NAME l_self_host WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-self-host -s local)

add_test(NAME f_add_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-add-flatware)
add_test(NAME f_return_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-return-flatware)
add_test(NAME f_helloworld_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-helloworld-flatware)
add_test(NAME f_open_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-open-flatware)

add_test(NAME f_build_python WORKING_DIRECTORY COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/applications-prefix/src/applications-build/ --target python_fixpoint --parallel)
add_test(NAME f_python_flatware WORKING_DIRECTORY COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tests/test-python-flatware)
set_tests_properties(f_build_python PROPERTIES FIXTURES_SETUP python_fixture)
set_tests_properties(f_python_flatware PROPERTIES FIXTURES_REQUIRED python_fixture)
