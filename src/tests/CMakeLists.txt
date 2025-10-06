link_libraries(glog)

# Unit Tests
add_executable(test-handle test-handle.cc unit-test-main.cc)
target_link_libraries(test-handle handle)

add_executable(test-blake3 test-blake3.cc unit-test-main.cc)
target_link_libraries(test-blake3 util glog)

add_executable(test-storage test-storage.cc unit-test-main.cc)
target_link_libraries(test-storage storage)

add_executable(test-hash-table test-hash-table.cc unit-test-main.cc)
target_link_libraries(test-hash-table storage)

add_executable(test-bptree test-bptree.cc unit-test-main.cc)

add_executable(hash-table-perf hash-table-perf.cc)
target_link_libraries(hash-table-perf storage)

add_executable(test-evaluator test-evaluator.cc unit-test-main.cc)
target_link_libraries(test-evaluator runtime)

add_executable(test-executor test-executor.cc unit-test-main.cc)
target_link_libraries(test-executor runtime)

add_executable(test-scheduler-relate test-scheduler-relate.cc unit-test-main.cc)
target_link_libraries(test-scheduler-relate runtime)

add_executable(test-distributed test-distributed.cc unit-test-main.cc)
target_link_libraries(test-distributed runtime)

add_executable(test-dependency-graph test-dependency-graph.cc unit-test-main.cc)
target_link_libraries(test-dependency-graph runtime)

add_executable(test-pass-scheduler test-pass-scheduler.cc unit-test-main.cc)
target_link_libraries(test-pass-scheduler runtime)

add_executable(test-local-scheduler test-local-scheduler.cc unit-test-main.cc)
target_link_libraries(test-local-scheduler runtime)

# Fixpoint/Flatware Tests
add_executable(test-add test-add.cc fixpoint-test-main.cc)
target_link_libraries(test-add runtime)

add_executable(test-fib test-fib.cc fixpoint-test-main.cc)
target_link_libraries(test-fib runtime)

add_executable(test-bptree-get test-bptree-get.cc fixpoint-test-main.cc)
target_link_libraries(test-bptree-get runtime)

add_executable(test-trap test-trap.cc fixpoint-test-main.cc)
target_link_libraries(test-trap runtime)

add_executable(test-resource-limits test-resource-limits.cc fixpoint-test-main.cc)
target_link_libraries(test-resource-limits runtime)

add_executable(test-map test-map.cc fixpoint-test-main.cc)
target_link_libraries(test-map runtime)

add_executable(test-curry test-curry.cc fixpoint-test-main.cc)
target_link_libraries(test-curry runtime)

add_executable(test-mapreduce test-mapreduce.cc fixpoint-test-main.cc)
target_link_libraries(test-mapreduce runtime)

add_executable(test-countwords test-countwords.cc fixpoint-test-main.cc)
target_link_libraries(test-countwords runtime)

add_executable(test-self-host test-self-host.cc fixpoint-test-main.cc)
target_link_libraries(test-self-host runtime)

add_executable(test-add-flatware test-add-flatware.cc fixpoint-test-main.cc)
target_link_libraries(test-add-flatware runtime)

add_executable(test-open-flatware test-open-flatware.cc fixpoint-test-main.cc)
target_link_libraries(test-open-flatware runtime)

add_executable(test-helloworld-flatware test-helloworld-flatware.cc fixpoint-test-main.cc)
target_link_libraries(test-helloworld-flatware runtime)

add_executable(test-return-flatware test-return-flatware.cc fixpoint-test-main.cc)
target_link_libraries(test-return-flatware runtime)

add_executable(test-python-flatware test-python-flatware.cc fixpoint-test-main.cc)
target_link_libraries(test-python-flatware runtime)
