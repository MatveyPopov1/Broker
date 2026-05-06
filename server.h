#ifndef SERVER_H
#define SERVER_H

#include "../queue/queue.h"
#include "../storage/storage.h"
#include "../metrics/metrics.h"
#include "../offset/offset.h"
#include "../subscriptions/subscriptions.h"
#include "../message/message.h"

#include <map>
#include <mutex>
#include <chrono>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstring>

class Server {
private:
    int server_fd;

    MessageQueue queue;
    Storage storage;
    Metrics metrics;

    OffsetManager offsetManager;
    SubscriptionManager subscriptionManager;

    std::map<std::string, int> activeConsumers;
    std::mutex conn_mtx;

    std::map<uint64_t, std::chrono::steady_clock::time_point> pendingAck;
    std::mutex ack_mtx;

    struct ClientState {
        int fd;
        std::string consumerName;
        std::string buffer;
    };

    std::string processCommand(ClientState& state, const std::string& command);
    void sendResponse(int client_fd, const std::string& response);

public:
    void start(int port);
    void acceptClients();
    void handleClient(int client_fd);

    void consumerThread();
    void retryThread();
};

#endif
