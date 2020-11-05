add_executable (tester "tester.cc")
target_link_libraries ("tester" util)

target_link_libraries ("tester" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("tester" ${ALSA_LIBRARIES})

target_link_libraries ("tester" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("tester" ${DBus_LIBRARIES})


add_executable (audio_sender "audio_sender.cc")
target_link_libraries ("audio_sender" util)

target_link_libraries ("audio_sender" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("audio_sender" ${ALSA_LIBRARIES})

target_link_libraries ("audio_sender" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("audio_sender" ${DBus_LIBRARIES})

add_executable (audio_receiver_test "audio_receiver_test.cc")
target_link_libraries ("audio_receiver_test" util)

target_link_libraries ("audio_receiver_test" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("audio_receiver_test" ${ALSA_LIBRARIES})

target_link_libraries ("audio_receiver_test" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("audio_receiver_test" ${DBus_LIBRARIES})