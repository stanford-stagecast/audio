#include "alsa_devices.hh"
#include <cstdlib>
#include <iostream>

using namespace std;

int main()
{
  ALSADevices devices;
  auto device_list = devices.list();

  for ( const auto& dev : device_list ) {
    cout << dev.name << " has these outputs:\n";
    for ( const auto& output : dev.outputs ) {
      cout << "     " << output.first << " = " << output.second << "\n";
    }
  }

  return EXIT_SUCCESS;
}
