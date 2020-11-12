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

target_link_libraries ("stagecast-client" ${ALSA_LDFLAGS})
target_link_libraries ("stagecast-client" ${ALSA_LDFLAGS_OTHER})

target_link_libraries ("stagecast-client" ${DBus_LDFLAGS})
target_link_libraries ("stagecast-client" ${DBus_LDFLAGS_OTHER})

target_link_libraries ("stagecast-client" ${Opus_LDFLAGS})
target_link_libraries ("stagecast-client" ${Opus_LDFLAGS_OTHER})

target_link_libraries ("stagecast-client" ${Rubberband_LDFLAGS})
target_link_libraries ("stagecast-client" ${Rubberband_LDFLAGS_OTHER})

if(BUILD_EMBEDDED_CLIENT)
add_executable (stagecast-client-embedded "stagecast-client-embedded.cc")
target_link_libraries ("stagecast-client-embedded" stats)
target_link_libraries ("stagecast-client-embedded" control)
target_link_libraries ("stagecast-client-embedded" playback)
target_link_libraries ("stagecast-client-embedded" network)
target_link_libraries ("stagecast-client-embedded" audio)
target_link_libraries ("stagecast-client-embedded" crypto)
target_link_libraries ("stagecast-client-embedded" util)

target_link_libraries ("stagecast-client-embedded" ${ALSA_LDFLAGS})
target_link_libraries ("stagecast-client-embedded" ${ALSA_LDFLAGS_OTHER})

target_link_libraries ("stagecast-client-embedded" "-Wl,--static")

target_link_libraries ("stagecast-client-embedded" ${Opus_LDFLAGS})
target_link_libraries ("stagecast-client-embedded" ${Opus_LDFLAGS_OTHER})

target_link_libraries ("stagecast-client-embedded" ${Rubberband_LDFLAGS})
target_link_libraries ("stagecast-client-embedded" ${Rubberband_LDFLAGS_OTHER})

target_link_libraries ("stagecast-client-embedded" "-pthread -Wl,-Bdynamic")
endif(BUILD_EMBEDDED_CLIENT)

add_executable (stagecast-server "stagecast-server.cc")
target_link_libraries ("stagecast-server" stats)
target_link_libraries ("stagecast-server" server)
target_link_libraries ("stagecast-server" playback)
target_link_libraries ("stagecast-server" network)
target_link_libraries ("stagecast-server" audio)
target_link_libraries ("stagecast-server" crypto)
target_link_libraries ("stagecast-server" util)

target_link_libraries ("stagecast-server" ${ALSA_LDFLAGS})
target_link_libraries ("stagecast-server" ${ALSA_LDFLAGS_OTHER})

target_link_libraries ("stagecast-server" ${Opus_LDFLAGS})
target_link_libraries ("stagecast-server" ${Opus_LDFLAGS_OTHER})

target_link_libraries ("stagecast-server" ${Sndfile_LDFLAGS})
target_link_libraries ("stagecast-server" ${Sndfile_LDFLAGS_OTHER})

target_link_libraries ("stagecast-server" ${AVFormat_LDFLAGS})
target_link_libraries ("stagecast-server" ${AVFormat_LDFLAGS_OTHER})

target_link_libraries ("stagecast-server" ${Rubberband_LDFLAGS})
target_link_libraries ("stagecast-server" ${Rubberband_LDFLAGS_OTHER})

add_executable (make-key "make-key.cc")
target_link_libraries ("make-key" network)
target_link_libraries ("make-key" crypto)
target_link_libraries ("make-key" util)

add_executable (client-control "client-control.cc")
target_link_libraries ("client-control" util)

add_executable (server-control "server-control.cc")
target_link_libraries ("server-control" util)

add_executable (audio-server "audio-server.cc")
target_link_libraries ("audio-server" stats)
target_link_libraries ("audio-server" http)
target_link_libraries ("audio-server" util)

target_link_libraries ("audio-server" ${SSL_LDFLAGS})
target_link_libraries ("audio-server" ${SSL_LDFLAGS_OTHER})

add_executable (ws-audio-server "ws-audio-server.cc")
target_link_libraries ("ws-audio-server" stats)
target_link_libraries ("ws-audio-server" http)
target_link_libraries ("ws-audio-server" util)

target_link_libraries ("ws-audio-server" ${SSL_LDFLAGS})
target_link_libraries ("ws-audio-server" ${SSL_LDFLAGS_OTHER})

target_link_libraries ("ws-audio-server" ${CryptoPP_LDFLAGS})
target_link_libraries ("ws-audio-server" ${CryptoPP_LDFLAGS_OTHER})

add_executable (example-http-server "example-http-server.cc")
target_link_libraries ("example-http-server" stats)
target_link_libraries ("example-http-server" http)
target_link_libraries ("example-http-server" util)

target_link_libraries ("example-http-server" ${SSL_LDFLAGS})
target_link_libraries ("example-http-server" ${SSL_LDFLAGS_OTHER})

add_executable (example-ws-server "example-ws-server.cc")
target_link_libraries ("example-ws-server" stats)
target_link_libraries ("example-ws-server" http)
target_link_libraries ("example-ws-server" util)

target_link_libraries ("example-ws-server" ${SSL_LDFLAGS})
target_link_libraries ("example-ws-server" ${SSL_LDFLAGS_OTHER})

target_link_libraries ("example-ws-server" ${CryptoPP_LDFLAGS})
target_link_libraries ("example-ws-server" ${CryptoPP_LDFLAGS_OTHER})

add_executable (webcam-demo "webcam-demo.cc")
target_link_libraries ("webcam-demo" video)
target_link_libraries ("webcam-demo" util)

target_link_libraries ("webcam-demo" ${V4L_LDFLAGS})
target_link_libraries ("webcam-demo" ${V4L_LDFLAGS_OTHER})

target_link_libraries ("webcam-demo" ${X264_LDFLAGS})
target_link_libraries ("webcam-demo" ${X264_LDFLAGS_OTHER})

target_link_libraries ("webcam-demo" "-pthread")
