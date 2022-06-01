enable_testing()

add_custom_target (check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^t_"
                         COMMENT "Testing...")

add_test(NAME t_return3 WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testing/return-test
  ${CMAKE_CURRENT_BINARY_DIR}/_deps/fixpoint-build/src/tester/wasi_tester
  ${CMAKE_CURRENT_BINARY_DIR}/src/examples/return3/output.wasm
)

add_test(NAME t_hello_world WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testing/hello-world-test
  ${CMAKE_CURRENT_BINARY_DIR}/_deps/fixpoint-build/src/tester/wasi_tester
  ${CMAKE_CURRENT_BINARY_DIR}/src/examples/helloworld/output.wasm
)
