file (GLOB LIB_SOURCES "*.cc")
add_library (http STATIC ${LIB_SOURCES})
target_include_directories (http PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries (http PRIVATE util)
