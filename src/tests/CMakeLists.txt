add_executable(test-add test-add.cc)
target_link_libraries(test-add ${FIXPOINT_LIBS})
target_link_libraries(test-add cryptopp wasmrt absl::flat_hash_map)

add_executable(test-fib test-fib.cc)
target_link_libraries(test-fib ${FIXPOINT_LIBS})
target_link_libraries(test-fib cryptopp wasmrt absl::flat_hash_map)

add_executable(test-trap test-trap.cc)
target_link_libraries(test-trap ${FIXPOINT_LIBS})
target_link_libraries(test-trap cryptopp wasmrt absl::flat_hash_map)

add_executable(test-add-flatware test-add-flatware.cc)
target_link_libraries(test-add-flatware ${FIXPOINT_LIBS})
target_link_libraries(test-add-flatware cryptopp wasmrt absl::flat_hash_map)

add_executable(test-open-flatware test-open-flatware.cc)
target_link_libraries(test-open-flatware ${FIXPOINT_LIBS})
target_link_libraries(test-open-flatware cryptopp wasmrt absl::flat_hash_map)

add_executable(test-return-flatware test-return-flatware.cc)
target_link_libraries(test-return-flatware ${FIXPOINT_LIBS})
target_link_libraries(test-return-flatware cryptopp wasmrt absl::flat_hash_map)

add_executable(test-helloworld-flatware test-helloworld-flatware.cc)
target_link_libraries(test-helloworld-flatware ${FIXPOINT_LIBS})
target_link_libraries(test-helloworld-flatware cryptopp wasmrt absl::flat_hash_map)

add_executable(test-map test-map.cc)
target_link_libraries(test-map ${FIXPOINT_LIBS})
target_link_libraries(test-map cryptopp wasmrt absl::flat_hash_map)
