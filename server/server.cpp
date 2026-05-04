#include "server/server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <vector>

void Server::start(int port) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    running = true;

    // Запускаем потребителя в отдельном потоке
    std::thread(&Server::consumerThread, this).detach();

    std::cout << "Server started on port " << port << std::endl;

    acceptClients();
}

void Server::stop() {
    running = false;
    queue.stop();
    if (server_fd >= 0) {
        close(server_fd);
    }
}

void Server::acceptClients() {
    while (running) {
        int client = accept(server_fd, nullptr, nullptr);

        if (client < 0) {
            // Либо остановка, либо ошибка
            break;
        }

        std::thread(&Server::handleClient, this, client).detach();
    }
}

void Server::handleClient(int client_fd) {
    char buffer[1024];

    while (running) {
        int bytes = read(client_fd, buffer, sizeof(buffer) - 1);

        if (bytes <= 0) break;

        buffer[bytes] = '\0';
        std::string msg(buffer);

        queue.push(msg);
        metrics.produced++;
    }

    close(client_fd);
}

void Server::consumerThread() {
    while (running) {
        // Пакетное извлечение: забираем до 100 сообщений за раз
        auto batch = queue.popBatch(100);

        if (batch.empty() && !running) {
            break;  // Очередь остановлена, выходим
        }

        for (const auto& msg : batch) {
            try {
                storage.store(msg);
                metrics.stored++;
            } catch (...) {
                std::cerr << "Failed to store message" << std::endl;
            }
        }

        // Не спамим — пауза 100 мс, чтобы не молотить CPU вхолостую
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Сохраняем оставшиеся сообщения перед выходом
    while (true) {
        auto batch = queue.popBatch(100);
        if (batch.empty()) break;

        for (const auto& msg : batch) {
            try {
                storage.store(msg);
                metrics.stored++;
            } catch (...) {
                std::cerr << "Failed to store message during shutdown" << std::endl;
            }
        }
    }

    metrics.print();
    std::cout << "Consumer thread finished." << std::endl;
}
