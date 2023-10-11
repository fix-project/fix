include_directories(${CMAKE_BINARY_DIR}/src/runtime)

add_executable(stateless-tester "stateless-tester.cc" "tester-utils.cc")
target_link_libraries(stateless-tester ${FIXPOINT_LIBS})
target_link_libraries(stateless-tester wasmrt absl::flat_hash_map)

add_executable(compiler-tester "compiler-tester.cc" "tester-utils.cc")
target_link_libraries(compiler-tester ${FIXPOINT_LIBS})
target_link_libraries(compiler-tester cryptopp wasmrt absl::flat_hash_map)

# add_executable(http-tester "http-tester.cc" "tester-utils.cc")
# target_link_libraries(http-tester ${FIXPOINT_LIBS})
# target_link_libraries(http-tester cryptopp wasmrt absl::flat_hash_map http)

# add_executable(dependency-tester "dependency-tester.cc" "tester-utils.cc")
# target_link_libraries(dependency-tester ${FIXPOINT_LIBS})
# target_link_libraries(dependency-tester cryptopp wasmrt absl::flat_hash_map)

add_executable(network-tester "network-tester.cc" "tester-utils.cc")
target_link_libraries(network-tester ${FIXPOINT_LIBS})
target_link_libraries(network-tester wasmrt absl::flat_hash_map)

add_executable(fixpoint-server "fixpoint-server.cc" "tester-utils.cc")
target_link_libraries(fixpoint-server ${FIXPOINT_LIBS})
target_link_libraries(fixpoint-server wasmrt absl::flat_hash_map)

add_executable(fixpoint-client "fixpoint-client.cc" "tester-utils.cc")
target_link_libraries(fixpoint-client ${FIXPOINT_LIBS})
target_link_libraries(fixpoint-client wasmrt absl::flat_hash_map)

# add_executable(distributed-tester "distributed-tester.cc" "tester-utils.cc")
# target_link_libraries(distributed-tester ${FIXPOINT_LIBS})
# target_link_libraries(distributed-tester cryptopp wasmrt absl::flat_hash_map)

# add_executable(distributed-worker "distributed-worker.cc" "tester-utils.cc")
# target_link_libraries(distributed-worker ${FIXPOINT_LIBS})
# target_link_libraries(distributed-worker cryptopp wasmrt absl::flat_hash_map)
