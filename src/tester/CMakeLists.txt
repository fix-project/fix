include_directories(${CMAKE_BINARY_DIR}/src/runtime)

add_executable(stateless-tester "stateless-tester.cc" "main.cc" "tester-utils.cc")
target_link_libraries(stateless-tester runtime)

add_executable(rw-tester "rw-tester.cc" "main.cc" "tester-utils.cc")
target_link_libraries(rw-tester runtime)

add_executable(fix "fix.cc" "main.cc" "tester-utils.cc")
target_link_libraries(fix runtime)

add_executable(compiler-tester "compiler-tester.cc" "main.cc" "tester-utils.cc")
target_link_libraries(compiler-tester runtime)

add_executable(fixpoint-server "fixpoint-server.cc" "main.cc" "tester-utils.cc")
target_link_libraries(fixpoint-server runtime)

find_program(TRUNK_EXECUTABLE, trunk)
find_program(CARGO_EXECUTABLE, cargo)

if ((NOT TRUNK_EXECUTABLE STREQUAL "TRUNK_EXECUTABLE,-NOTFOUND") AND (NOT CARGO_EXECUTABLE STREQUAL "CARGO_EXECUTABLE,-NOTFOUND") )
  add_executable(http-tester "http-tester.cc" "main.cc" "tester-utils.cc")
  target_link_libraries(http-tester runtime http)

  execute_process(
    COMMAND trunk build --dist "${CMAKE_BINARY_DIR}/src/tester/http-client"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/tester/http-client"
  )
endif()

add_executable(fixpoint-client "fixpoint-client.cc" "main.cc" "tester-utils.cc")
target_link_libraries(fixpoint-client runtime)
