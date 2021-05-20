set_source_files_properties(
    "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o" PROPERTIES
    EXTERNAL_OBJECT TRUE
    GENERATED TRUE
)

add_executable(add_tester "add_tester.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_link_libraries("add_tester" runtime compiler runtime compiler)
target_link_libraries("add_tester" component)
target_link_libraries("add_tester" util)
target_link_libraries("add_tester" cryptopp)
target_link_libraries("add_tester" wasmrt)
target_link_libraries("add_tester" ${CLANG_LIBS})
target_link_libraries("add_tester" LLVM)
target_link_libraries("add_tester" absl::flat_hash_map )
target_link_libraries("add_tester" wabt)
add_dependencies("add_tester" wasm2c)

add_executable(fib_tester "fib_tester.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_link_libraries("fib_tester" runtime compiler runtime compiler)
target_link_libraries("fib_tester" component)
target_link_libraries("fib_tester" util)
target_link_libraries("fib_tester" cryptopp)
target_link_libraries("fib_tester" wasmrt)
target_link_libraries("fib_tester" ${CLANG_LIBS})
target_link_libraries("fib_tester" LLVM)
target_link_libraries("fib_tester" absl::flat_hash_map )
target_link_libraries("fib_tester" wabt)
add_dependencies("fib_tester" wasm2c)

add_executable(c_to_elf_tester "c_to_elf_tester.cc")
target_link_libraries("c_to_elf_tester" util)
target_link_libraries("c_to_elf_tester" compiler)
target_link_libraries("c_to_elf_tester" ${CLANG_LIBS})
target_link_libraries("c_to_elf_tester" LLVM)
