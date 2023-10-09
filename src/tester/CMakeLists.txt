include_directories(${CMAKE_BINARY_DIR}/src/runtime)

add_executable(wasi_tester "wasi_tester.cc")
target_link_libraries(wasi_tester ${FIXPOINT_LIBS})
target_link_libraries(wasi_tester wasmrt absl::flat_hash_map)

add_executable(stateless-tester "stateless-tester.cc" "tester-utils.cc")
target_link_libraries(stateless-tester ${FIXPOINT_LIBS})
target_link_libraries(stateless-tester wasmrt absl::flat_hash_map)

add_executable(compiler-tester "compiler-tester.cc" "tester-utils.cc")
target_link_libraries(compiler-tester ${FIXPOINT_LIBS})
target_link_libraries(compiler-tester wasmrt absl::flat_hash_map)

add_executable(http-tester "http-tester.cc" "tester-utils.cc")
target_link_libraries(http-tester ${FIXPOINT_LIBS})
target_link_libraries(http-tester wasmrt absl::flat_hash_map http)

add_executable(dependency-tester "dependency-tester.cc" "tester-utils.cc")
target_link_libraries(dependency-tester ${FIXPOINT_LIBS})
target_link_libraries(dependency-tester wasmrt absl::flat_hash_map)

add_executable(network-tester "network-tester.cc" "tester-utils.cc")
target_link_libraries(network-tester ${FIXPOINT_LIBS})
target_link_libraries(network-tester wasmrt absl::flat_hash_map)

add_executable(add_timing "add_timing.cc")
target_compile_definitions(add_timing PUBLIC TIME_FIXPOINT=1)
target_link_libraries(add_timing ${FIXPOINT_LIBS_TIMING})
target_link_libraries(add_timing wasmrt absl::flat_hash_map)

add_library(add_obj OBJECT add.cc)

add_executable(benchmark_timing "benchmark_timing.cc")
target_compile_definitions(benchmark_timing PUBLIC TIME_FIXPOINT=1)
target_link_libraries(benchmark_timing ${FIXPOINT_LIBS_TIMING})
target_link_libraries(benchmark_timing wasmrt absl::flat_hash_map)
target_link_libraries(benchmark_timing add_obj)

add_executable(step_timing "step_timing.cc")
target_compile_definitions(step_timing PUBLIC TIME_FIXPOINT=2)
target_link_libraries(step_timing ${FIXPOINT_LIBS_TIMING_2})
target_link_libraries(step_timing wasmrt absl::flat_hash_map)
target_link_libraries(step_timing add_obj)

add_executable(benchmark_timing_perf "benchmark_timing_perf.cc")
target_compile_definitions(benchmark_timing_perf PUBLIC TIME_FIXPOINT=1)
target_link_libraries(benchmark_timing_perf ${FIXPOINT_LIBS_TIMING})
target_link_libraries(benchmark_timing_perf wasmrt absl::flat_hash_map)
target_link_libraries(benchmark_timing_perf add_obj)

add_executable(base64_test "base64_test.cc")
target_link_libraries(base64_test component util)

add_executable(serialization_test "serialization_test.cc")
target_link_libraries(serialization_test ${FIXPOINT_LIBS})
target_link_libraries(serialization_test wasmrt absl::flat_hash_map)
target_link_libraries(serialization_test add_obj)

add_executable(serialization_test_deep "serialization_test_deep.cc")
target_link_libraries(serialization_test_deep ${FIXPOINT_LIBS})
target_link_libraries(serialization_test_deep wasmrt absl::flat_hash_map)
target_link_libraries(serialization_test_deep add_obj)

add_executable(deserialization_test "deserialization_test.cc")
target_link_libraries(deserialization_test ${FIXPOINT_LIBS})
target_link_libraries(deserialization_test wasmrt absl::flat_hash_map)
target_link_libraries(deserialization_test add_obj)
