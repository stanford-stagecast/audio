#include "crypto.h"

using namespace Crypto;

int main()
{
    char* test_string = "This is a test.";
    
    Crypto::CryptoException crypto;
    cout << crypto.text << endl;
    return 0;
}