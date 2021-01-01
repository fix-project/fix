set_source_files_properties(
    "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o" PROPERTIES
    EXTERNAL_OBJECT TRUE
    GENERATED TRUE
)

add_executable(compiler_tester "compiler_tester.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_link_libraries("compiler_tester" util)
target_link_libraries("compiler_tester" compiler)
target_link_libraries("compiler_tester" wabt)
add_dependencies("compiler_tester" wasm2c)

add_executable(c_to_elf_tester "c_to_elf_tester.cc")
target_link_libraries("c_to_elf_tester" compiler)
target_link_libraries("c_to_elf_tester" ${CLANG_LIBS})
target_link_libraries("c_to_elf_tester" LLVM)

add_executable(temp_tester "temp_tester.cc")
target_link_libraries("temp_tester" util)
target_link_libraries("temp_tester" compiler)
target_link_libraries("temp_tester" ${CLANG_LIBS})
target_link_libraries("temp_tester" LLVM)

add_executable(wasm_to_elf_tester "wasm_to_elf_tester.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-build/CMakeFiles/wasm2c.dir/src/c-writer.cc.o")
target_link_libraries("wasm_to_elf_tester" util)
target_link_libraries("wasm_to_elf_tester" compiler)
target_link_libraries("wasm_to_elf_tester" ${CLANG_LIBS})
target_link_libraries("wasm_to_elf_tester" LLVM)
target_link_libraries("wasm_to_elf_tester" wabt)
add_dependencies("wasm_to_elf_tester" wasm2c)

add_executable(vfs_tester "llvm_vfs_tester.cc")
target_link_libraries("vfs_tester" LLVM)
