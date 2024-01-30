file (GLOB LIB_SOURCES "*.cc")
add_library (wasmrt STATIC ${LIB_SOURCES})
target_include_directories(wasmrt INTERFACE .)
