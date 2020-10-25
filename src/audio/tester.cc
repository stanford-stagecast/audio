#include "alsa_devices.hh"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace std;
using namespace std::chrono;

pair<string, string> find_device( const string_view expected_description )
{
  ALSADevices devices;
  bool found = false;

  string name, interface_name;

  for ( const auto& dev : devices.list() ) {
    for ( const auto& interface : dev.interfaces ) {
      if ( interface.second == expected_description ) {
        if ( found ) {
          throw runtime_error( "Multiple devices matching description" );
        } else {
          found = true;
          name = dev.name;
          interface_name = interface.first;
        }
      }
    }
  }

  if ( not found ) {
    throw runtime_error( "Device \"" + string( expected_description ) + "\" not found" );
  }

  return { name, interface_name };
}

void program_body()
{
  ios::sync_with_stdio( false );

  const auto [name, interface_name] = find_device( "UAC-2, USB Audio" );

  cout << "Found " << interface_name << " as " << name << "\n";

  try {
    AudioDeviceClaim ownership { name };

    cout << "Claimed ownership of " << name;
    if ( ownership.claimed_from() ) {
      cout << " from " << ownership.claimed_from().value();
    }
    cout << endl;
  } catch ( const exception& e ) {
    cout << "Failed to claim ownership: " << e.what() << "\n";
  }

  AudioInterface headphone { interface_name, "Headphone", SND_PCM_STREAM_PLAYBACK };
  AudioInterface microphone { interface_name, "Microphone", SND_PCM_STREAM_CAPTURE };

  microphone.loopback_to( headphone );
}

int main()
{
  try {
    program_body();
  } catch ( const exception& e ) {
    cout << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
