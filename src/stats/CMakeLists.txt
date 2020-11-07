add_executable (sender "sender.cc")
target_link_libraries ("sender" util)

add_executable (receiver "receiver.cc")
target_link_libraries ("receiver" util)

add_executable (server "server.cc")
target_link_libraries ("server" util)

target_link_libraries ("sender" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("sender" ${ALSA_LIBRARIES})

target_link_libraries ("sender" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("sender" ${DBus_LIBRARIES})

target_link_libraries ("receiver" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("receiver" ${ALSA_LIBRARIES})

target_link_libraries ("receiver" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("receiver" ${DBus_LIBRARIES})
