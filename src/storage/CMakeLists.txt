add_library (storage STATIC runtimestorage.cc repository.cc hash_table.cc)
target_include_directories (storage INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(storage PUBLIC handle component util glog absl::flat_hash_map)
