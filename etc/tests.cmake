enable_testing()

add_custom_target (fixpoint-check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "^t_"
  COMMENT "Testing..."
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/file.txt
)

add_custom_command (OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/file.txt
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/src/tester/serialization_test_deep | cut -c28- | head -c -1 > ${CMAKE_CURRENT_BINARY_DIR}/file.txt
)

file(READ ${PROJECT_SOURCE_DIR}/boot/compile-encode COMPILE-ENCODE)

message(PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
message(COMPILE-ENCODE="${COMPILE-ENCODE}")

add_test(NAME t_deep_open WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/applications/flatware/testing/open-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/stateless-tester
  ${COMPILE-ENCODE}
  ${CMAKE_CURRENT_BINARY_DIR}/file.txt
  ${CMAKE_CURRENT_BINARY_DIR}/applications-prefix/src/applications-build/flatware/examples/open/open-deep-fixpoint.wasm
)

add_test(NAME t_open WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/applications/flatware/testing/open-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/stateless-tester
  ${COMPILE-ENCODE}
  ${CMAKE_CURRENT_BINARY_DIR}/file.txt
  ${CMAKE_CURRENT_BINARY_DIR}/applications-prefix/src/applications-build/flatware/examples/open/open-fixpoint.wasm
)

add_test(NAME t_addblob WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testing/add-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/stateless-tester
  ${COMPILE-ENCODE}
  ${CMAKE_CURRENT_BINARY_DIR}/testing/wasm-examples/addblob.wasm
)

add_test(NAME t_add_simple WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testing/add-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/stateless-tester
  ${COMPILE-ENCODE}
  ${CMAKE_CURRENT_BINARY_DIR}/testing/wasm-examples/add-simple.wasm
)

add_test(NAME t_fib WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testing/fib-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/stateless-tester
  ${COMPILE-ENCODE}
  ${CMAKE_CURRENT_BINARY_DIR}/testing/wasm-examples/fib.wasm
  ${CMAKE_CURRENT_BINARY_DIR}/testing/wasm-examples/add-simple.wasm
)

add_test(NAME t_return3 WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/applications/flatware/testing/return-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/stateless-tester
  ${COMPILE-ENCODE}
  ${CMAKE_CURRENT_BINARY_DIR}/applications-prefix/src/applications-build/flatware/examples/return3/return3-fixpoint.wasm
)

add_test(NAME t_hello_world WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/applications/flatware/testing/hello-world-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/stateless-tester
  ${COMPILE-ENCODE}
  ${CMAKE_CURRENT_BINARY_DIR}/applications-prefix/src/applications-build/flatware/examples/helloworld/helloworld-fixpoint.wasm
)

add_test(NAME t_add_args WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/applications/flatware/testing/add-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/stateless-tester
  ${COMPILE-ENCODE}
  ${CMAKE_CURRENT_BINARY_DIR}/applications-prefix/src/applications-build/flatware/examples/add/add-fixpoint.wasm
)

add_test(NAME t_trap WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testing/expect-trap
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/stateless-tester
  ${COMPILE-ENCODE}
  ${CMAKE_CURRENT_BINARY_DIR}/testing/wasm-examples/trap.wasm
  "Integer divide by zero"
)

add_test(NAME t_map WORKING_DIRECTORY
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/applications/testing/map-test
  ${CMAKE_CURRENT_BINARY_DIR}/src/tester/stateless-tester
  ${COMPILE-ENCODE}
  ${CMAKE_CURRENT_BINARY_DIR}/applications-prefix/src/applications-build/map/add_2_map.wasm
)
