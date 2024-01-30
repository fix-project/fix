file (GLOB LIB_SOURCES "*.cc")
add_library (util STATIC ${LIB_SOURCES})
target_link_libraries(util PUBLIC blake3)
