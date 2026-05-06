#include "doctest.h"
#include "../server/server.h"
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

static std::string sendCommand(const std::string& cmd) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8081);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    connect(sock, (sockaddr*)&addr, sizeof(addr));

    char buffer[4096];
    int bytes = read(sock, buffer, sizeof(buffer) - 1);
    buffer[bytes] = '\0';
    std::string welcome(buffer);

    std::string fullCmd = cmd + "\n";
    write(sock, fullCmd.c_str(), fullCmd.size());

    bytes = read(sock, buffer, sizeof(buffer) - 1);
    buffer[bytes] = '\0';

    close(sock);
    return std::string(buffer);
}

TEST_CASE("Server auth flow") {
    std::remove("users.dat");
    std::remove("broker.log");

    Server server;
    server.setInteractive(false);
    server.setConsoleLogging(false);

    std::thread serverThread([&server]() {
        server.start(8081);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    SUBCASE("Register and login") {
        std::string regResponse = sendCommand("REGISTER testuser testpass");
        CHECK(regResponse.find("OK") != std::string::npos);

        std::string loginResponse = sendCommand("LOGIN testuser testpass");
        CHECK(loginResponse.find("OK") != std::string::npos);
    }

    SUBCASE("Login with wrong password") {
        sendCommand("REGISTER wronguser correctpass");
        std::string response = sendCommand("LOGIN wronguser wrongpass");
        CHECK(response.find("неверный пароль") != std::string::npos);
    }

    SUBCASE("Register duplicate user") {
        sendCommand("REGISTER dupuser pass1");
        std::string response = sendCommand("REGISTER dupuser pass2");
        CHECK(response.find("уже существует") != std::string::npos);
    }

    server.stop();
    serverThread.join();
    std::remove("users.dat");
    std::remove("broker.log");
}
