#ifndef SERVER_H
#define SERVER_H

#include "../queue/queue.h"
#include "../storage/storage.h"
#include "../metrics/metrics.h"
#include <map>
#include <vector>

extern std::map<std::string, std::vector<int>> subscribers;
extern std::mutex sub_mtx;

class Server {
private:
    int server_fd;

    MessageQueue queue;
    Storage storage;
    Metrics metrics;

public:
    void start(int port);

    void acceptClients();
    void handleClient(int client_fd);

    void consumerThread();
};

#endif