#include <stdio.h>
#include <iostream>
#include <sstream>

#include "crypto.h"

using namespace Crypto;

int main( int argc, char *argv[] )
{
  if ( argc != 2 ) {
    fprintf( stderr, "Usage: %s NONCE\n", argv[ 0 ] );
    return 1;
  }

  try {
    Base64Key key;
    Session session( key );
    Nonce nonce( myatoi( argv[ 1 ] ) );

    /* Read input */
    std::ostringstream input;
    input << std::cin.rdbuf();

    /* Encrypt message */

    string ciphertext = session.encrypt( Message( nonce, input.str() ) );

    std::cerr << "Key: " << key.printable_key() << std::endl;

    std::cout << ciphertext;
  } catch ( const CryptoException &e ) {
    std::cerr << e.what() << std::endl;
    exit( 1 );
  }

  return 0;
}