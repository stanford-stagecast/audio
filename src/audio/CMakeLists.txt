add_executable (tester "tester.cc")
target_link_libraries ("tester" util)

target_link_libraries ("tester" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("tester" ${ALSA_LIBRARIES})

target_link_libraries ("tester" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("tester" ${DBus_LIBRARIES})

add_executable (audio_sender_test "audio_sender_test.cc")
target_link_libraries ("audio_sender_test" util)

target_link_libraries ("audio_sender_test" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("audio_sender_test" ${ALSA_LIBRARIES})

target_link_libraries ("audio_sender_test" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("audio_sender_test" ${DBus_LIBRARIES})

add_executable (audio_receiver "audio_receiver.cc")
target_link_libraries ("audio_receiver" util)

target_link_libraries ("audio_receiver" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("audio_receiver" ${ALSA_LIBRARIES})

target_link_libraries ("audio_receiver" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("audio_receiver" ${DBus_LIBRARIES})
