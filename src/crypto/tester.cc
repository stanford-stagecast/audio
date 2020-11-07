#include "crypto.h"

using namespace Crypto;

int main()
{
    char* test_string = "This is a test.";
    Session::Session session;
    session.encrypt(test_string, );
    session.decrypt(test_string, );

    cout << crypto.text << endl;
    return 0;
}