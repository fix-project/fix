include_directories(${CMAKE_BINARY_DIR}/src/runtime)

add_executable(stateless-tester "stateless-tester.cc" "main.cc" "main.cc" "tester-utils.cc")
target_link_libraries(stateless-tester runtime)

add_executable(compiler-tester "compiler-tester.cc" "main.cc" "tester-utils.cc")
target_link_libraries(compiler-tester runtime)

add_executable(sha-tester "sha-tester.cc" "main.cc")
target_link_libraries(sha-tester runtime)
target_link_libraries(sha-tester util)


add_executable(fixpoint-server "fixpoint-server.cc" "main.cc" "tester-utils.cc")
target_link_libraries(fixpoint-server runtime)

add_executable(fixpoint-client "fixpoint-client.cc" "main.cc" "tester-utils.cc")
target_link_libraries(fixpoint-client runtime)
