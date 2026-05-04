#pragma once
#include "storage/storage.h"
#include "storage/queue.h"
#include "storage/metrics.h"
#include <atomic>

class Server {
private:
    int server_fd;
    Storage storage;
    SafeQueue queue;
    Metrics metrics;
    std::atomic<bool> running{false};

    void acceptClients();
    void handleClient(int client_fd);
    void consumerThread();

public:
    void start(int port);
    void stop();  // новый метод для корректного завершения
};
