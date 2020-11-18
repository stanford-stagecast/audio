cmake_minimum_required ( VERSION 2.8.5 )
cmake_policy ( SET CMP0054 NEW )
project ( StageCast-Audio )

include ( etc/build_type.cmake )
include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )

find_package ( PkgConfig )

pkg_check_modules ( ALSA REQUIRED alsa )
include_directories ( ${ALSA_INCLUDE_DIRS} )
add_compile_options ( ${ALSA_CFLAGS_OTHER} )

pkg_check_modules ( DBus REQUIRED dbus-1 )
include_directories ( ${DBus_INCLUDE_DIRS} )
add_compile_options ( ${DBus_CFLAGS_OTHER} )

pkg_check_modules ( Opus REQUIRED opus )
include_directories ( ${Opus_INCLUDE_DIRS} )
add_compile_options ( ${Opus_CFLAGS_OTHER} )

include_directories ("${PROJECT_SOURCE_DIR}/src/util")
include_directories ("${PROJECT_SOURCE_DIR}/src/crypto")
include_directories ("${PROJECT_SOURCE_DIR}/src/audio")
include_directories ("${PROJECT_SOURCE_DIR}/src/network")
include_directories ("${PROJECT_SOURCE_DIR}/src/dbus")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src/util")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/crypto")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/audio")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/network")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/dbus")
#add_subdirectory ("${PROJECT_SOURCE_DIR}/src/stats")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/frontend")
