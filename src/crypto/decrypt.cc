#include <stdio.h>
#include <iostream>
#include <sstream>

#include "crypto.h"

using namespace Crypto;

int main( int argc, char *argv[] )
{
  if ( argc != 2 ) {
    fprintf( stderr, "Usage: %s KEY\n", argv[ 0 ] );
    return 1;
  }

  try {
    Base64Key key( argv[ 1 ] );
    Session session( key );

    /* Read input */
    std::ostringstream input;
    input << std::cin.rdbuf();

    /* Decrypt message */

    Message message = session.decrypt( input.str() );

    fprintf( stderr, "Nonce = %ld\n",
	     (long)message.nonce.val() );
    std::cout << message.text;
  } catch ( const CryptoException &e ) {
    std::cerr << e.what() << std::endl;
    exit( 1 );
  }

  return 0;
}