#include "server/server.h"
#include <csignal>
#include <iostream>

Server* globalServer = nullptr;

void signalHandler(int signum) {
    std::cout << "\nShutting down... (signal " << signum << ")" << std::endl;
    if (globalServer) {
        globalServer->stop();
    }
}

int main() {
    Server server;
    globalServer = &server;

    signal(SIGINT, signalHandler);   // Ctrl+C
    signal(SIGTERM, signalHandler);  // kill

    server.start(8080);

    std::cout << "Server stopped." << std::endl;
    return 0;
}
