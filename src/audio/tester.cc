#include "alsa_devices.hh"
#include <cstdlib>
#include <iostream>

using namespace std;

pair<string, string> find_device( const string_view expected_description )
{
  ALSADevices devices;
  bool found = false;

  string name, output_name;

  for ( const auto& dev : devices.list() ) {
    for ( const auto& output : dev.outputs ) {
      if ( output.second == expected_description ) {
        if ( found ) {
          throw runtime_error( "Multiple devices matching description" );
        } else {
          found = true;
          name = dev.name;
          output_name = output.first;
        }
      }
    }
  }

  if ( not found ) {
    throw runtime_error( "device \"" + string( expected_description ) + "\" not found" );
  }

  return { name, output_name };
}

void program_body()
{
  const auto [name, output_name] = find_device( "UAC-2, USB Audio" );

  cout << "Device found, nice name is " << name << " and output name is " << output_name << "\n";
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
