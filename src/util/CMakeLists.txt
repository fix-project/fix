file (GLOB LIB_SOURCES "*.cc")

add_library (util STATIC ${LIB_SOURCES})
target_link_libraries(util PUBLIC blake3)
target_include_directories (util SYSTEM PUBLIC "${PROJECT_SOURCE_DIR}/third-party")
target_include_directories (util INTERFACE .)
target_include_directories (util PUBLIC "${PROJECT_SOURCE_DIR}/src/handle")
