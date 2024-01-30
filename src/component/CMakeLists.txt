file (GLOB LIB_SOURCES "*.cc")
add_library (component STATIC ${LIB_SOURCES})
target_include_directories(component PUBLIC ../runtime ../wasm-rt) # this is kind of messy...
target_link_libraries(component PRIVATE util)
target_link_libraries(component PUBLIC handle)
target_link_libraries(component PRIVATE absl::flat_hash_map)
target_include_directories(component INTERFACE .)
