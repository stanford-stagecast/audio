add_library (stats STATIC "stats_printer.cc")

add_executable (stagecast-client "stagecast-client.cc")
target_link_libraries ("stagecast-client" stats)
target_link_libraries ("stagecast-client" dbus)
target_link_libraries ("stagecast-client" control)
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

if(BUILD_EMBEDDED_CLIENT)
add_executable (stagecast-client-embedded "stagecast-client-embedded.cc")
target_link_libraries ("stagecast-client-embedded" stats)
target_link_libraries ("stagecast-client-embedded" control)
target_link_libraries ("stagecast-client-embedded" playback)
target_link_libraries ("stagecast-client-embedded" network)
target_link_libraries ("stagecast-client-embedded" audio)
target_link_libraries ("stagecast-client-embedded" crypto)
target_link_libraries ("stagecast-client-embedded" util)

target_link_libraries ("stagecast-client-embedded" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("stagecast-client-embedded" ${ALSA_LIBRARIES})

target_link_libraries ("stagecast-client-embedded" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("stagecast-client-embedded" "-Wl,--static")
target_link_libraries ("stagecast-client-embedded" ${Opus_LIBRARIES})
target_link_libraries ("stagecast-client-embedded" "-Wl,-Bdynamic")
endif(BUILD_EMBEDDED_CLIENT)

add_executable (stagecast-server "stagecast-server.cc")
target_link_libraries ("stagecast-server" stats)
target_link_libraries ("stagecast-server" server)
target_link_libraries ("stagecast-server" playback)
target_link_libraries ("stagecast-server" network)
target_link_libraries ("stagecast-server" audio)
target_link_libraries ("stagecast-server" crypto)
target_link_libraries ("stagecast-server" util)

target_link_libraries ("stagecast-server" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("stagecast-server" ${ALSA_LIBRARIES})

target_link_libraries ("stagecast-server" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("stagecast-server" ${Opus_LIBRARIES})

target_link_libraries ("stagecast-server" ${Sndfile_LDFLAGS_OTHER})
target_link_libraries ("stagecast-server" ${Sndfile_LIBRARIES})

target_link_libraries ("stagecast-server" ${AVFormat_LDFLAGS_OTHER})
target_link_libraries ("stagecast-server" ${AVFormat_LIBRARIES})

target_link_libraries ("stagecast-server" ${AVUtil_LDFLAGS_OTHER})
target_link_libraries ("stagecast-server" ${AVUtil_LIBRARIES})

add_executable (make-key "make-key.cc")
target_link_libraries ("make-key" network)
target_link_libraries ("make-key" crypto)
target_link_libraries ("make-key" util)

add_executable (client-control "client-control.cc")
target_link_libraries ("client-control" util)

add_executable (server-control "server-control.cc")
target_link_libraries ("server-control" util)
