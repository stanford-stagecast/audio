cmake_minimum_required ( VERSION 2.8.5 )
cmake_policy ( SET CMP0054 NEW )
project ( StageCast-Audio )

include ( etc/build_defs.cmake )
include ( etc/build_type.cmake )
include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/clang_tidy.cmake )

file ( GLOB CLIENT_SRC
    src/stats/sender.cc
    src/util/*
)

file ( GLOB SERVER_SRC
    src/stats/receiver.cc
    src/util/*
)

include_directories ( 
    src/util
    src/stats
)

add_executable (client ${CLIENT_SRC})
add_executable (server ${SERVER_SRC})
target_link_libraries (server pthread)
#add_library (audio STATIC
#	${AUDIO_SRC}
#)

set ( AUDIO_LIBS
    audio
)
