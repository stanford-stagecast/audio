#include "alsa_devices.hh"
#include <cstdlib>
#include <optional>
#include <unistd.h>

#ifndef NDBUS
#include "audio_device_claim.hh"
#endif

using namespace std;

pair<string, string> find_device( const string_view expected_description );
optional<AudioDeviceClaim> try_claim_ownership( const string_view name );