link_libraries(glog)

# Unit Tests
add_executable(test-handle test-handle.cc main.cc)
target_link_libraries(test-handle handle)

add_executable(test-blake3 test-blake3.cc main.cc)
target_link_libraries(test-blake3 util glog)

add_executable(test-storage test-storage.cc main.cc)
target_link_libraries(test-storage storage)

add_executable(test-evaluator test-evaluator.cc main.cc)
target_link_libraries(test-evaluator runtime)

add_executable(test-executor test-executor.cc main.cc)
target_link_libraries(test-executor runtime)

add_executable(test-relater test-relater.cc main.cc)
target_link_libraries(test-relater runtime)

add_executable(test-distributed test-distributed.cc main.cc)
target_link_libraries(test-distributed runtime)

add_executable(test-dependency-graph test-dependency-graph.cc main.cc)
target_link_libraries(test-dependency-graph runtime)

add_executable(test-scheduler test-scheduler.cc main.cc)
target_link_libraries(test-scheduler runtime)

add_executable(test-add test-add.cc main.cc)
target_link_libraries(test-add runtime)

add_executable(test-fib test-fib.cc main.cc)
target_link_libraries(test-fib runtime)

add_executable(test-trap test-trap.cc main.cc)
target_link_libraries(test-trap runtime)

add_executable(test-resource-limits test-resource-limits.cc main.cc)
target_link_libraries(test-resource-limits runtime)

add_executable(test-map test-map.cc main.cc)
target_link_libraries(test-map runtime)

add_executable(test-curry test-curry.cc main.cc)
target_link_libraries(test-curry runtime)

add_executable(test-mapreduce test-mapreduce.cc main.cc)
target_link_libraries(test-mapreduce runtime)

add_executable(test-countwords test-countwords.cc main.cc)
target_link_libraries(test-countwords runtime)

add_executable(test-self-host test-self-host.cc main.cc)
target_link_libraries(test-self-host runtime)

add_executable(test-add-flatware test-add-flatware.cc main.cc)
target_link_libraries(test-add-flatware runtime)

add_executable(test-open-flatware test-open-flatware.cc main.cc)
target_link_libraries(test-open-flatware runtime)

add_executable(test-helloworld-flatware test-helloworld-flatware.cc main.cc)
target_link_libraries(test-helloworld-flatware runtime)

add_executable(test-return-flatware test-return-flatware.cc main.cc)
target_link_libraries(test-return-flatware runtime)

add_executable(test-python-flatware test-python-flatware.cc main.cc)
target_link_libraries(test-python-flatware runtime)

add_executable(test-filesys-flatware test-filesys-flatware.cc main.cc)
target_link_libraries(test-filesys-flatware runtime)

