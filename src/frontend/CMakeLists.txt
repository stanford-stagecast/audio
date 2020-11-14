add_executable (tester "tester.cc")
target_link_libraries ("tester" util)
target_link_libraries ("tester" audio)
target_link_libraries ("tester" dbus)

target_link_libraries ("tester" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("tester" ${ALSA_LIBRARIES})

target_link_libraries ("tester" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("tester" ${DBus_LIBRARIES})

target_link_libraries ("tester" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("tester" ${Opus_LIBRARIES})
