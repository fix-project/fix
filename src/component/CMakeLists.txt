file (GLOB LIB_SOURCES "*.cc")
add_library (component STATIC ${LIB_SOURCES})
add_library (component-timing STATIC ${LIB_SOURCES})
target_compile_definitions(component-timing PUBLIC TIME_FIXPOINT=1)
