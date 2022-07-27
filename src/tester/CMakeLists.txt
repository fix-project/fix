set_source_files_properties(
    "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o" PROPERTIES
    EXTERNAL_OBJECT TRUE
    GENERATED TRUE
)

add_executable(add_tester "add_tester.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_link_libraries(add_tester ${FIXPOINT_LIBS})
target_link_libraries(add_tester cryptopp wasmrt ${CLANG_LIBS} LLVM absl::flat_hash_map wabt)
add_dependencies(add_tester wasm2c)

add_executable(fib_tester "fib_tester.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_link_libraries(fib_tester ${FIXPOINT_LIBS})
target_link_libraries(fib_tester cryptopp wasmrt ${CLANG_LIBS} LLVM absl::flat_hash_map wabt)
add_dependencies(fib_tester wasm2c)

add_executable(wasi_tester "wasi_tester.cc" "option-parser.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_link_libraries(wasi_tester ${FIXPOINT_LIBS})
target_link_libraries(wasi_tester cryptopp wasmrt ${CLANG_LIBS} LLVM absl::flat_hash_map wabt)
add_dependencies(wasi_tester wasm2c)

add_executable(wasm_to_c_tester "wasm_to_c_tester.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_link_libraries(wasm_to_c_tester ${FIXPOINT_LIBS})
target_link_libraries(wasm_to_c_tester ${CLANG_LIBS} LLVM wabt)
add_dependencies(wasm_to_c_tester wasm2c)

add_executable(wasm_to_elf_tester "wasm_to_elf_tester.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_link_libraries(wasm_to_elf_tester ${FIXPOINT_LIBS})
target_link_libraries(wasm_to_elf_tester ${CLANG_LIBS} LLVM wabt)
add_dependencies(wasm_to_elf_tester wasm2c)

add_executable(add_timing "add_timing.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_compile_definitions(add_timing PUBLIC TIME_FIXPOINT=1)
target_link_libraries(add_timing ${FIXPOINT_LIBS_TIMING})
target_link_libraries(add_timing cryptopp wasmrt ${CLANG_LIBS} LLVM absl::flat_hash_map wabt)
add_dependencies(add_timing wasm2c)

add_library(add_obj OBJECT add.cc)

add_executable(benchmark_timing "benchmark_timing.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_compile_definitions(benchmark_timing PUBLIC TIME_FIXPOINT=1)
target_link_libraries(benchmark_timing ${FIXPOINT_LIBS_TIMING})
target_link_libraries(benchmark_timing cryptopp wasmrt ${CLANG_LIBS} LLVM absl::flat_hash_map wabt)
add_dependencies(benchmark_timing wasm2c)
target_link_libraries(benchmark_timing add_obj)

add_executable(step_timing "step_timing.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_compile_definitions(step_timing PUBLIC TIME_FIXPOINT=2)
target_link_libraries(step_timing ${FIXPOINT_LIBS_TIMING_2})
target_link_libraries(step_timing cryptopp wasmrt ${CLANG_LIBS} LLVM absl::flat_hash_map wabt)
add_dependencies(step_timing wasm2c)
target_link_libraries(step_timing add_obj)

add_executable(benchmark_timing_perf "benchmark_timing_perf.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_compile_definitions(benchmark_timing_perf PUBLIC TIME_FIXPOINT=1)
target_link_libraries(benchmark_timing_perf ${FIXPOINT_LIBS_TIMING})
target_link_libraries(benchmark_timing_perf cryptopp wasmrt ${CLANG_LIBS} LLVM absl::flat_hash_map wabt)
add_dependencies(benchmark_timing_perf wasm2c)
target_link_libraries(benchmark_timing_perf add_obj)

add_executable(base64_test "base64_test.cc")
target_link_libraries(base64_test component util)
target_link_libraries(base64_test cryptopp)

add_executable(serialization_test "serialization_test.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_link_libraries(serialization_test ${FIXPOINT_LIBS})
target_link_libraries(serialization_test cryptopp wasmrt ${CLANG_LIBS} LLVM absl::flat_hash_map wabt)
add_dependencies(serialization_test wasm2c)
target_link_libraries(serialization_test add_obj)

add_executable(deserialization_test "deserialization_test.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_link_libraries(deserialization_test ${FIXPOINT_LIBS})
target_link_libraries(deserialization_test cryptopp wasmrt ${CLANG_LIBS} LLVM absl::flat_hash_map wabt)
add_dependencies(deserialization_test wasm2c)
target_link_libraries(deserialization_test add_obj)

