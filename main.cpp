#include "server/server.h"

int main() {
    Server server;
    server.start(8080);
    return 0;
}