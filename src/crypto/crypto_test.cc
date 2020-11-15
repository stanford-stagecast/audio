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
  Message decrypted_msg = session->decrypt(encrypted_str.c_str(), sizeof(encrypted_str));

  std::cout << decrypted_msg.text << std::endl;
  return 0;
}
