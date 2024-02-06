file (GLOB LIB_SOURCES evaluator.cc executor.cc message.cc network.cc fixpointapi.cc elfloader.cc applyer.cc no-apply-executor.cc)

add_library (runtime STATIC ${LIB_SOURCES})
target_include_directories (runtime PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(runtime PUBLIC storage handle component util glog absl::flat_hash_map PRIVATE wasmrt)
