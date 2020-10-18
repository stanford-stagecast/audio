#include "alsa_devices.hh"
#include <cstdlib>

int main()
{
  ALSADevices devices;
  devices.list_devices();

  return EXIT_SUCCESS;
}
