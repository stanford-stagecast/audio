add_library (stats STATIC "stats_printer.cc")

add_executable (stagecast-client "stagecast-client.cc")
target_link_libraries ("stagecast-client" stats)
#target_link_libraries ("stagecast-client" dbus)
target_link_libraries ("stagecast-client" playback)
target_link_libraries ("stagecast-client" network)
target_link_libraries ("stagecast-client" audio)
target_link_libraries ("stagecast-client" crypto)
target_link_libraries ("stagecast-client" util)

target_link_libraries ("stagecast-client" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("stagecast-client" ${ALSA_LIBRARIES})

target_link_libraries ("stagecast-client" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("stagecast-client" ${DBus_LIBRARIES})

target_link_libraries ("stagecast-client" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("stagecast-client" ${Opus_LIBRARIES})

target_link_libraries ("stagecast-client" ${Sndfile_LDFLAGS_OTHER})
target_link_libraries ("stagecast-client" ${Sndfile_LIBRARIES})

add_executable (dummy_server "dummy_server.cc")
target_link_libraries ("dummy_server" stats)
target_link_libraries ("dummy_server" playback)
target_link_libraries ("dummy_server" network)
target_link_libraries ("dummy_server" audio)
target_link_libraries ("dummy_server" crypto)
target_link_libraries ("dummy_server" util)

target_link_libraries ("dummy_server" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("dummy_server" ${ALSA_LIBRARIES})

target_link_libraries ("dummy_server" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("dummy_server" ${Opus_LIBRARIES})

target_link_libraries ("dummy_server" ${Sndfile_LDFLAGS_OTHER})
target_link_libraries ("dummy_server" ${Sndfile_LIBRARIES})

add_executable (unit_test "unit_test.cc")
target_link_libraries ("unit_test" stats)
target_link_libraries ("unit_test" network)
target_link_libraries ("unit_test" audio)
target_link_libraries ("unit_test" crypto)
target_link_libraries ("unit_test" util)

target_link_libraries ("unit_test" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("unit_test" ${ALSA_LIBRARIES})

target_link_libraries ("unit_test" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("unit_test" ${Opus_LIBRARIES})

add_executable (stagecast-server "stagecast-server.cc")
target_link_libraries ("stagecast-server" stats)
target_link_libraries ("stagecast-server" playback)
target_link_libraries ("stagecast-server" network)
target_link_libraries ("stagecast-server" audio)
target_link_libraries ("stagecast-server" crypto)
target_link_libraries ("stagecast-server" util)

target_link_libraries ("stagecast-server" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("stagecast-server" ${ALSA_LIBRARIES})

target_link_libraries ("stagecast-server" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("stagecast-server" ${Opus_LIBRARIES})
