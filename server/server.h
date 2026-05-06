#ifndef SERVER_H
#define SERVER_H

#include "../queue/queue.h"
#include "../storage/storage.h"
#include "../metrics/metrics.h"
#include "../offset/offset.h"
#include "../subscriptions/subscriptions.h"
#include "../message/message.h"
#include "../user/user.h"
#include "../logger/logger.h"

#include <map>
#include <mutex>
#include <chrono>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

class Server {
private:
    int server_fd;
    bool running;

    MessageQueue queue;
    Storage storage;
    Metrics metrics;
    Logger logger;

    OffsetManager offsetManager;
    SubscriptionManager subscriptionManager;
    UserManager userManager;

    std::map<std::string, int> activeConsumers;
    std::mutex conn_mtx;

    struct PendingMessage {
        std::set<std::string> waitingConsumers;
        std::string topic;
        std::chrono::steady_clock::time_point lastRetry;
        int retryCount;
    };

    std::map<uint64_t, PendingMessage> pendingAck;
    std::mutex ack_mtx;

    struct ClientState {
        int fd;
        std::string consumerName;
        std::string buffer;
    };

    std::string getWelcomeMessage();
    std::string getHelpMessage(bool authenticated);
    std::string processCommand(ClientState& state, const std::string& command);
    std::string processAuthCommand(ClientState& state, const std::string& command);
    void sendResponse(int client_fd, const std::string& response);
    void deliverMessage(const Message& msg);
    void replayHistory(const std::string& consumerName, int client_fd);
    void disconnectClient(ClientState& state);

public:
    void start(int port);
    void acceptClients();
    void handleClient(int client_fd);

    void consumerThread();
    void retryThread();
    void compactThread();
    void consoleThread();
    void stop();
};

#endif
