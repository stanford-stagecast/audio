add_executable (crypto_test "crypto_test.cc")
target_link_libraries ("crypto_test" util)

target_link_libraries ("crypto_test" ${ALSA_LDFLAGS_OTHER})
target_link_libraries ("crypto_test" ${ALSA_LIBRARIES})

target_link_libraries ("crypto_test" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("crypto_test" ${DBus_LIBRARIES})

target_link_libraries ("crypto_test" ${Opus_LDFLAGS_OTHER})
target_link_libraries ("crypto_test" ${Opus_LIBRARIES})
