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
