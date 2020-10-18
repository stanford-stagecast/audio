cmake_minimum_required ( VERSION 2.8.5 )
cmake_policy ( SET CMP0054 NEW )
project ( StageCast-Audio )

include ( etc/build_defs.cmake )
include ( etc/build_type.cmake )
include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/clang_tidy.cmake )

include_directories ("${PROJECT_SOURCE_DIR}/src/util")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src/util")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/stats")
