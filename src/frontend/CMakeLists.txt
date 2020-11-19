add_library (stats STATIC "stats_printer.cc")

add_executable (tester "tester.cc")
target_link_libraries ("tester" stats)
target_link_libraries ("tester" dbus)
target_link_libraries ("tester" network)
target_link_libraries ("tester" audio)
target_link_libraries ("tester" crypto)
target_link_libraries ("tester" util)

target_link_libraries ("tester" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("tester" ${ALSA_LIBRARIES})

target_link_libraries ("tester" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("tester" ${DBus_LIBRARIES})

target_link_libraries ("tester" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("tester" ${Opus_LIBRARIES})

add_executable (dummy_server "dummy_server.cc")
target_link_libraries ("dummy_server" stats)
target_link_libraries ("dummy_server" network)
target_link_libraries ("dummy_server" audio)
target_link_libraries ("dummy_server" crypto)
target_link_libraries ("dummy_server" util)

target_link_libraries ("dummy_server" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("dummy_server" ${ALSA_LIBRARIES})

target_link_libraries ("dummy_server" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("dummy_server" ${Opus_LIBRARIES})

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
