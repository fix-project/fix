add_executable(bootstrap "bootstrap.cc")
target_link_libraries(bootstrap ${FIXPOINT_LIBS})
target_link_libraries(bootstrap wasmrt absl::flat_hash_map)
