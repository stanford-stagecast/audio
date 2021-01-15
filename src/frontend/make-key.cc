#include <cstdlib>
#include <iostream>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "file_descriptor.hh"
#include "keys.hh"

using namespace std;

void program_body( const string& filename, const string& keyname )
{
  ios::sync_with_stdio( false );

  LongLivedKey k { keyname };

  string output( k.serialized_length(), 0 );
  Serializer s { string_span::from_view( output ) };
  k.serialize( s );

  FileDescriptor file { CheckSystemCall(
    "open( \"" + filename + "\" )", open( filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR ) ) };

  string_view remaining_to_write = output;

  while ( not remaining_to_write.empty() ) {
    remaining_to_write.remove_prefix( file.write( output ) );
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 3 ) {
      cerr << "Usage: " << argv[0] << " filename keyname\n";
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2] );
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
