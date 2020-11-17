add_executable (tester "tester.cc" "stats_printer.cc")
target_link_libraries ("tester" dbus)
target_link_libraries ("tester" network)
target_link_libraries ("tester" audio)
target_link_libraries ("tester" util)

target_link_libraries ("tester" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("tester" ${ALSA_LIBRARIES})

target_link_libraries ("tester" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("tester" ${DBus_LIBRARIES})

target_link_libraries ("tester" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("tester" ${Opus_LIBRARIES})

add_executable (dummy_server "dummy_server.cc" "stats_printer.cc")
target_link_libraries ("dummy_server" network)
target_link_libraries ("dummy_server" audio)
target_link_libraries ("dummy_server" util)

target_link_libraries ("dummy_server" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("dummy_server" ${ALSA_LIBRARIES})

target_link_libraries ("dummy_server" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("dummy_server" ${Opus_LIBRARIES})

add_executable (unit_test "unit_test.cc" "stats_printer.cc")
target_link_libraries ("unit_test" network)
target_link_libraries ("unit_test" audio)
target_link_libraries ("unit_test" util)

target_link_libraries ("unit_test" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("unit_test" ${ALSA_LIBRARIES})

target_link_libraries ("unit_test" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("unit_test" ${Opus_LIBRARIES})
