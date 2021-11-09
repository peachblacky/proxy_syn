#include "client.h"

bool client::isSupported(string str) {
    return ( (str.compare("GET") == 0) || (str.compare("HEAD") == 0) );
}