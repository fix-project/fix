add_executable(test-add test-add.cc main.cc)
target_link_libraries(test-add runtime)

add_executable(test-fib test-fib.cc main.cc)
target_link_libraries(test-fib runtime)

add_executable(test-trap test-trap.cc main.cc)
target_link_libraries(test-trap runtime)

add_executable(test-add-flatware test-add-flatware.cc main.cc)
target_link_libraries(test-add-flatware runtime)

add_executable(test-open-flatware test-open-flatware.cc main.cc)
target_link_libraries(test-open-flatware runtime)

add_executable(test-return-flatware test-return-flatware.cc main.cc)
target_link_libraries(test-return-flatware runtime)

add_executable(test-helloworld-flatware test-helloworld-flatware.cc main.cc)
target_link_libraries(test-helloworld-flatware runtime)

add_executable(test-map test-map.cc main.cc)
target_link_libraries(test-map runtime)

add_executable(test-storage test-storage.cc main.cc)
target_link_libraries(test-storage runtime)
