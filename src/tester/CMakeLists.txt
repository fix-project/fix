add_executable(compiler_tester "compiler_tester.cc" "${CMAKE_BINARY_DIR}/_deps/wabt-src/src/c-writer.cc")
target_link_libraries("compiler_tester" util)
target_link_libraries("compiler_tester" compiler)
target_link_libraries("compiler_tester" wabt)
