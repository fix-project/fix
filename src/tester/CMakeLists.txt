include_directories(${CMAKE_BINARY_DIR}/src/runtime)

add_executable(fix "fix.cc" "tester-utils.cc")
target_link_libraries(fix runtime storage)
target_include_directories (fix PUBLIC "${PROJECT_SOURCE_DIR}/src/tests")

add_executable(fixpoint-server "fixpoint-server.cc" "tester-utils.cc")
target_link_libraries(fixpoint-server runtime)

add_executable(data-server "data-server.cc" "tester-utils.cc")
target_link_libraries(data-server runtime)

add_executable(fixpoint-client "fixpoint-client.cc" "main.cc" "tester-utils.cc")
target_link_libraries(fixpoint-client runtime)

add_executable(fixpoint-client-wikipedia "fixpoint-client-wikipedia.cc" "tester-utils.cc")
target_link_libraries(fixpoint-client-wikipedia runtime)

add_executable(fixpoint-client-compile "fixpoint-client-compile.cc" "tester-utils.cc")
target_link_libraries(fixpoint-client-compile runtime)

add_executable(fixpoint-client-bptree "fixpoint-client-bptree.cc" "tester-utils.cc")
target_link_libraries(fixpoint-client-bptree runtime)

add_executable(fixpoint-client-oneoff "fixpoint-client-oneoff.cc" "tester-utils.cc")
target_link_libraries(fixpoint-client-oneoff runtime)

add_executable(fixpoint-client-dec "fixpoint-client-dec.cc" "tester-utils.cc")
target_link_libraries(fixpoint-client-dec runtime)

# add_executable(http-tester "http-tester.cc" "main.cc" "tester-utils.cc")
# target_link_libraries(http-tester runtime http)
