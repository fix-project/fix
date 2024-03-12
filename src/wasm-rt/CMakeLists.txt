file (GLOB LIB_SOURCES "*.cc")
add_library (wasmrt STATIC ${LIB_SOURCES})
target_include_directories(wasmrt INTERFACE .)
target_link_libraries(wasmrt PRIVATE util)
