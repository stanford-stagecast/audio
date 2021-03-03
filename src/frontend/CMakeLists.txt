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

target_link_libraries ("stagecast-client" ${JSON_LDFLAGS})
target_link_libraries ("stagecast-client" ${JSON_LDFLAGS_OTHER})

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

target_link_libraries ("stagecast-client-embedded" ${JSON_LDFLAGS})
target_link_libraries ("stagecast-client-embedded" ${JSON_LDFLAGS_OTHER})

target_link_libraries ("stagecast-client-embedded" "-pthread -Wl,-Bdynamic")

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

target_link_libraries ("stagecast-server" ${Rubberband_LDFLAGS})
target_link_libraries ("stagecast-server" ${Rubberband_LDFLAGS_OTHER})

target_link_libraries ("stagecast-server" ${JSON_LDFLAGS})
target_link_libraries ("stagecast-server" ${JSON_LDFLAGS_OTHER})

add_executable (make-key "make-key.cc")
target_link_libraries ("make-key" network)
target_link_libraries ("make-key" crypto)
target_link_libraries ("make-key" util)

add_executable (client-control "client-control.cc")
target_link_libraries ("client-control" util)

add_executable (server-control "server-control.cc")
target_link_libraries ("server-control" util)

add_executable (video-server-control "video-server-control.cc")
target_link_libraries ("video-server-control" util)

add_executable (audio-server "audio-server.cc")
target_link_libraries ("audio-server" stats)
target_link_libraries ("audio-server" http)
target_link_libraries ("audio-server" util)

target_link_libraries ("audio-server" ${SSL_LDFLAGS})
target_link_libraries ("audio-server" ${SSL_LDFLAGS_OTHER})

add_executable (ws-audio-server "ws-audio-server.cc")
target_link_libraries ("ws-audio-server" stats)
target_link_libraries ("ws-audio-server" playback)
target_link_libraries ("ws-audio-server" http)
target_link_libraries ("ws-audio-server" util)

target_link_libraries ("ws-audio-server" ${SSL_LDFLAGS})
target_link_libraries ("ws-audio-server" ${SSL_LDFLAGS_OTHER})

target_link_libraries ("ws-audio-server" ${AVFormat_LDFLAGS})
target_link_libraries ("ws-audio-server" ${AVFormat_LDFLAGS_OTHER})

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

add_executable (example-video-server "example-video-server.cc")
target_link_libraries ("example-video-server" stats)
target_link_libraries ("example-video-server" util)

target_link_libraries ("example-video-server" ${SSL_LDFLAGS})
target_link_libraries ("example-video-server" ${SSL_LDFLAGS_OTHER})

add_executable (stagecast-video-client "stagecast-video-client.cc")
target_link_libraries ("stagecast-video-client" stats)
target_link_libraries ("stagecast-video-client" video)
target_link_libraries ("stagecast-video-client" control)
target_link_libraries ("stagecast-video-client" playback)
target_link_libraries ("stagecast-video-client" network)
target_link_libraries ("stagecast-video-client" audio)
target_link_libraries ("stagecast-video-client" crypto)
target_link_libraries ("stagecast-video-client" util)

target_link_libraries ("stagecast-video-client" "-no-pie")

target_link_libraries ("stagecast-video-client" "-Wl,--static")

target_link_libraries ("stagecast-video-client" ${V4L_LDFLAGS})
target_link_libraries ("stagecast-video-client" ${V4L_LDFLAGS_OTHER})

target_link_libraries ("stagecast-video-client" ${AVFormat_LDFLAGS})
target_link_libraries ("stagecast-video-client" ${AVFormat_LDFLAGS_OTHER})

target_link_libraries ("stagecast-video-client" ${Opus_LDFLAGS})
target_link_libraries ("stagecast-video-client" ${Opus_LDFLAGS_OTHER})

target_link_libraries ("stagecast-video-client" "-Wl,-Bdynamic")

target_link_libraries ("stagecast-video-client" "-pthread")

target_link_libraries ("stagecast-video-client" ${X264_LDFLAGS})
target_link_libraries ("stagecast-video-client" ${X264_LDFLAGS_OTHER})

add_executable (stagecast-video-server "stagecast-video-server.cc")
target_link_libraries ("stagecast-video-server" stats)
target_link_libraries ("stagecast-video-server" video)
target_link_libraries ("stagecast-video-server" server)
target_link_libraries ("stagecast-video-server" playback)
target_link_libraries ("stagecast-video-server" network)
target_link_libraries ("stagecast-video-server" audio)
target_link_libraries ("stagecast-video-server" crypto)
target_link_libraries ("stagecast-video-server" util)

target_link_libraries ("stagecast-video-server" ${ALSA_LDFLAGS})
target_link_libraries ("stagecast-video-server" ${ALSA_LDFLAGS_OTHER})

target_link_libraries ("stagecast-video-server" ${Opus_LDFLAGS})
target_link_libraries ("stagecast-video-server" ${Opus_LDFLAGS_OTHER})

target_link_libraries ("stagecast-video-server" ${Sndfile_LDFLAGS})
target_link_libraries ("stagecast-video-server" ${Sndfile_LDFLAGS_OTHER})

target_link_libraries ("stagecast-video-server" ${V4L_LDFLAGS})
target_link_libraries ("stagecast-video-server" ${V4L_LDFLAGS_OTHER})

target_link_libraries ("stagecast-video-server" ${AVFormat_LDFLAGS})
target_link_libraries ("stagecast-video-server" ${AVFormat_LDFLAGS_OTHER})

target_link_libraries ("stagecast-video-server" ${AVCodec_LDFLAGS})
target_link_libraries ("stagecast-video-server" ${AVCodec_LDFLAGS_OTHER})

target_link_libraries ("stagecast-video-server" ${Rubberband_LDFLAGS})
target_link_libraries ("stagecast-video-server" ${Rubberband_LDFLAGS_OTHER})

target_link_libraries ("stagecast-video-server" ${X264_LDFLAGS})
target_link_libraries ("stagecast-video-server" ${X264_LDFLAGS_OTHER})

target_link_libraries ("stagecast-video-server" ${JSON_LDFLAGS})
target_link_libraries ("stagecast-video-server" ${JSON_LDFLAGS_OTHER})

target_link_libraries ("stagecast-video-server" "-pthread")

add_executable (ws-camera-server "ws-camera-server.cc")
target_link_libraries ("ws-camera-server" stats)
target_link_libraries ("ws-camera-server" video)
target_link_libraries ("ws-camera-server" http)
target_link_libraries ("ws-camera-server" network)
target_link_libraries ("ws-camera-server" crypto)
target_link_libraries ("ws-camera-server" util)

target_link_libraries ("ws-camera-server" ${SSL_LDFLAGS})
target_link_libraries ("ws-camera-server" ${SSL_LDFLAGS_OTHER})

target_link_libraries ("ws-camera-server" ${CryptoPP_LDFLAGS})
target_link_libraries ("ws-camera-server" ${CryptoPP_LDFLAGS_OTHER})

target_link_libraries ("ws-camera-server" ${AVFormat_LDFLAGS})
target_link_libraries ("ws-camera-server" ${AVFormat_LDFLAGS_OTHER})

add_executable (ws-switcher-server "ws-switcher-server.cc")
target_link_libraries ("ws-switcher-server" stats)
target_link_libraries ("ws-switcher-server" video)
target_link_libraries ("ws-switcher-server" http)
target_link_libraries ("ws-switcher-server" network)
target_link_libraries ("ws-switcher-server" crypto)
target_link_libraries ("ws-switcher-server" util)

target_link_libraries ("ws-switcher-server" ${SSL_LDFLAGS})
target_link_libraries ("ws-switcher-server" ${SSL_LDFLAGS_OTHER})

target_link_libraries ("ws-switcher-server" ${CryptoPP_LDFLAGS})
target_link_libraries ("ws-switcher-server" ${CryptoPP_LDFLAGS_OTHER})

target_link_libraries ("ws-switcher-server" ${AVFormat_LDFLAGS})
target_link_libraries ("ws-switcher-server" ${AVFormat_LDFLAGS_OTHER})

add_executable (webcam-demo "webcam-demo.cc")
target_link_libraries ("webcam-demo" stats)
target_link_libraries ("webcam-demo" video)
target_link_libraries ("webcam-demo" util)

target_link_libraries ("webcam-demo" ${V4L_LDFLAGS})
target_link_libraries ("webcam-demo" ${V4L_LDFLAGS_OTHER})

target_link_libraries ("webcam-demo" ${AVFormat_LDFLAGS})
target_link_libraries ("webcam-demo" ${AVFormat_LDFLAGS_OTHER})

target_link_libraries ("webcam-demo" ${X264_LDFLAGS})
target_link_libraries ("webcam-demo" ${X264_LDFLAGS_OTHER})

add_executable (webcam-demo-ws "webcam-demo-ws.cc")
target_link_libraries ("webcam-demo-ws" stats)
target_link_libraries ("webcam-demo-ws" video)
target_link_libraries ("webcam-demo-ws" http)
target_link_libraries ("webcam-demo-ws" util)

target_link_libraries ("webcam-demo-ws" ${CryptoPP_LDFLAGS})
target_link_libraries ("webcam-demo-ws" ${CryptoPP_LDFLAGS_OTHER})

target_link_libraries ("webcam-demo-ws" ${V4L_LDFLAGS})
target_link_libraries ("webcam-demo-ws" ${V4L_LDFLAGS_OTHER})

target_link_libraries ("webcam-demo-ws" ${AVFormat_LDFLAGS})
target_link_libraries ("webcam-demo-ws" ${AVFormat_LDFLAGS_OTHER})

target_link_libraries ("webcam-demo-ws" ${X264_LDFLAGS})
target_link_libraries ("webcam-demo-ws" ${X264_LDFLAGS_OTHER})

add_executable (ws-control-server "ws-control-server.cc")
target_link_libraries ("ws-control-server" stats)
target_link_libraries ("ws-control-server" playback)
target_link_libraries ("ws-control-server" http)
target_link_libraries ("ws-control-server" util)

target_link_libraries ("ws-control-server" ${SSL_LDFLAGS})
target_link_libraries ("ws-control-server" ${SSL_LDFLAGS_OTHER})

target_link_libraries ("ws-control-server" ${CryptoPP_LDFLAGS})
target_link_libraries ("ws-control-server" ${CryptoPP_LDFLAGS_OTHER})

add_executable (media-download "media-download.cc")
target_link_libraries ("media-download" util)

target_link_libraries ("media-download" ${JSON_LDFLAGS})
target_link_libraries ("media-download" ${JSON_LDFLAGS_OTHER})

add_executable (cue-parser "cue-parser.cc")
target_link_libraries ("cue-parser" video)
target_link_libraries ("cue-parser" util)

target_link_libraries ("cue-parser" ${JSON_LDFLAGS})
target_link_libraries ("cue-parser" ${JSON_LDFLAGS_OTHER})

target_link_libraries ("cue-parser" ${V4L_LDFLAGS})
target_link_libraries ("cue-parser" ${V4L_LDFLAGS_OTHER})

target_link_libraries ("cue-parser" "-pthread")
