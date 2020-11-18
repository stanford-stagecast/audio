file (GLOB LIB_SOURCES "*.cc")
add_library (crypto STATIC ${LIB_SOURCES})

set_source_files_properties(ocb.cc PROPERTIES COMPILE_FLAGS "-Wno-cast-qual -Wno-unused-variable -Wno-implicit-fallthrough -Wno-unused-function")
