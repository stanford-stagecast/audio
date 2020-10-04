cmake_minimum_required ( VERSION 2.8.5 )
cmake_policy ( SET CMP0054 NEW )
project ( StageCast-Audio )

include ( FindJPEG )

find_package ( JPEG REQUIRED )
find_package ( OpenGL REQUIRED )
find_package ( GLEW REQUIRED )
find_package ( glfw3 REQUIRED )


include ( etc/build_defs.cmake )
include ( etc/build_type.cmake )
include ( etc/cflags.cmake )

include ( etc/clang_format.cmake )
include ( etc/clang_tidy.cmake )

file ( GLOB AUDIO_SRC
    src/*
)

include_directories ( src )

add_library ( audio STATIC
	${AUDIO_SRC}
)

set ( AUDIO_LIBS
    audio
    glfw
    GLEW
    GL
    GLU
    jpeg
)
