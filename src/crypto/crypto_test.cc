#include "crypto.h"
#include <string.h>
#include <iostream>
using namespace Crypto;

#define NONCE_NUM 10

int main()
{
  string test_string = "This is a test.";
  Base64Key* key = new Base64Key();
  Session* session = new Session(*key);
  Nonce* nonce = new Nonce(NONCE_NUM);
  Message* msg = new Message(*nonce, test_string);
  string encrypted_str = session->encrypt(*msg);
  //TODO: Next line is incorrect and needs to be fixed
  Message decrypted_msg = session->decrypt(encrypted_str.c_str(), strlen(encrypted_str.c_str()));
  (void) decrypted_msg;
  /*std::cout << decrypted_msg.text << std::endl;*/
  return 0;
}
