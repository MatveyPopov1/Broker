#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <csignal>

Server* globalServer = nullptr;

void signalHandler(int signum) {
    if (globalServer) {
        std::cout << "\nReceived signal " << signum << std::endl;
        globalServer->stop();
    }
}

void Server::start(int port) {
    globalServer = this;
    running = true;

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    offsetManager.load();
    subscriptionManager.load();

    std::thread(&Server::consumerThread, this).detach();
    std::thread(&Server::retryThread, this).detach();
    std::thread(&Server::compactThread, this).detach();
    std::thread(&Server::consoleThread, this).detach();

    std::cout << "Server started on port " << port << std::endl;
    std::cout << "Type 'stop' to shutdown server" << std::endl;

    acceptClients();
}

void Server::consoleThread() {
    std::string input;
    while (running) {
        std::getline(std::cin, input);

        if (input == "stop") {
            std::cout << "Shutdown command received from console" << std::endl;
            stop();
            break;
        }
    }
}

void Server::disconnectClient(ClientState& state) {
    std::string name = state.consumerName;

    if (!name.empty()) {
        {
            std::lock_guard<std::mutex> lock(conn_mtx);
            activeConsumers.erase(name);
        }

        offsetManager.save();

        std::cout << "Consumer disconnected: " << name << std::endl;
    }

    shutdown(state.fd, SHUT_RDWR);
    close(state.fd);
}

void Server::stop() {
    std::cout << "\nShutting down server..." << std::endl;
    running = false;

    {
        std::lock_guard<std::mutex> lock(conn_mtx);

        for (auto& pair : activeConsumers) {
            std::string msg = "SYS server shutting down\n";
            write(pair.second, msg.c_str(), msg.size());
            shutdown(pair.second, SHUT_RDWR);
            close(pair.second);
        }

        activeConsumers.clear();
    }

    storage.compact();
    std::cout << "Final compaction completed" << std::endl;

    close(server_fd);
    exit(0);
}

void Server::acceptClients() {
    while (running) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) continue;

        std::thread(&Server::handleClient, this, client).detach();
    }
}

void Server::sendResponse(int client_fd, const std::string& response) {
    std::string out = response + "\n";
    write(client_fd, out.c_str(), out.size());
}

void Server::handleClient(int client_fd) {
    ClientState state;
    state.fd = client_fd;
    state.buffer = "";

    char temp[1024];

    sendResponse(client_fd, "SYS connected to server");

    while (running) {
        int bytes = read(client_fd, temp, sizeof(temp) - 1);
        if (bytes <= 0) {
            disconnectClient(state);
            break;
        }

        temp[bytes] = '\0';
        state.buffer += std::string(temp);

        size_t pos;
        while ((pos = state.buffer.find('\n')) != std::string::npos) {
            std::string command = state.buffer.substr(0, pos);
            state.buffer.erase(0, pos + 1);

            if (!command.empty() && command.back() == '\r') {
                command.pop_back();
            }

            if (!command.empty()) {
                std::string response = processCommand(state, command);

                if (response == "__DISCONNECT__") {
                    sendResponse(client_fd, "OK goodbye");
                    disconnectClient(state);
                    return;
                }

                if (!response.empty()) {
                    sendResponse(client_fd, response);
                }
            }
        }
    }

    close(client_fd);
}

std::string Server::processCommand(ClientState& state, const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    if (cmd == "PING") {
        return "PONG";
    }

    if (cmd == "DISCONNECT") {
        return "__DISCONNECT__";
    }

    if (cmd == "CONNECT") {
        std::string consumerName;
        iss >> consumerName;

        if (consumerName.empty()) {
            return "ERROR usage: CONNECT <consumer_name>";
        }

        state.consumerName = consumerName;

        std::lock_guard<std::mutex> lock(conn_mtx);
        activeConsumers[consumerName] = state.fd;

        std::cout << "Consumer connected: " << consumerName << std::endl;

        replayHistory(consumerName, state.fd);

        return "OK connected as " + consumerName;
    }

    if (state.consumerName.empty()) {
        return "ERROR you must CONNECT first";
    }

    if (cmd == "SUB") {
        std::string topic, consumer;
        iss >> topic >> consumer;

        if (topic.empty() || consumer.empty()) {
            return "ERROR usage: SUB <topic> <consumer>";
        }

        bool added = subscriptionManager.add(topic, consumer);

        if (added) {
            std::cout << consumer << " subscribed to " << topic << std::endl;
            return "OK subscribed to " + topic;
        } else {
            return "ERROR already subscribed to " + topic;
        }
    }

    if (cmd == "UNSUB") {
        std::string topic, consumer;
        iss >> topic >> consumer;

        if (topic.empty() || consumer.empty()) {
            return "ERROR usage: UNSUB <topic> <consumer>";
        }

        bool removed = subscriptionManager.remove(topic, consumer);

        if (removed) {
            std::cout << consumer << " unsubscribed from " << topic << std::endl;
            return "OK unsubscribed from " + topic;
        } else {
            return "ERROR not subscribed to " + topic;
        }
    }

    if (cmd == "LIST_TOPICS") {
        auto topics = subscriptionManager.getTopics();

        if (topics.empty()) {
            return "OK no topics available";
        }

        std::string response = "OK topics:";
        for (auto& t : topics) {
            response += " " + t;
        }
        return response;
    }

    if (cmd == "LIST_ACTIVE_TOPICS") {
        auto topics = subscriptionManager.getActiveTopics();

        if (topics.empty()) {
            return "OK no active topics";
        }

        std::string response = "OK active topics:";
        for (auto& t : topics) {
            response += " " + t;
        }
        return response;
    }

    if (cmd == "LIST_SUBS") {
        std::string consumer;
        iss >> consumer;

        if (consumer.empty()) {
            return "ERROR usage: LIST_SUBS <consumer>";
        }

        auto subs = subscriptionManager.getSubscriptions(consumer);

        if (subs.empty()) {
            return "OK no active subscriptions for " + consumer;
        }

        std::string response = "OK subscriptions for " + consumer + ":";
        for (auto& s : subs) {
            response += " " + s;
        }
        return response;
    }

    if (cmd == "PUB") {
        std::string topic;
        iss >> topic;

        if (topic.empty()) {
            return "ERROR usage: PUB <topic> <message> <priority>";
        }

        std::string rest;
        std::getline(iss, rest);

        while (!rest.empty() && rest.front() == ' ') {
            rest.erase(0, 1);
        }

        size_t lastSpace = rest.find_last_of(' ');
        if (lastSpace == std::string::npos) {
            return "ERROR usage: PUB <topic> <message> <priority>";
        }

        std::string body = rest.substr(0, lastSpace);
        std::string priorityStr = rest.substr(lastSpace + 1);

        int priority;
        try {
            priority = std::stoi(priorityStr);
        } catch (...) {
            return "ERROR priority must be integer";
        }

        subscriptionManager.ensureTopic(topic);

        Message msg;
        msg.topic = topic;
        msg.body = body;
        msg.priority = priority;

        queue.push(msg);
        metrics.produced++;

        std::cout << "Message published to " << topic << " priority " << priority << std::endl;
        return "OK message published to " + topic;
    }

    if (cmd == "ACK") {
        std::string consumer;
        uint64_t id;

        iss >> id >> consumer;

        if (consumer.empty()) {
            return "ERROR usage: ACK <id> <consumer>";
        }

        bool allAcked = false;

        {
            std::lock_guard<std::mutex> lock(ack_mtx);

            auto it = pendingAck.find(id);
            if (it != pendingAck.end()) {
                it->second.waitingConsumers.erase(consumer);

                std::cout << "ACK " << id << " from " << consumer
                          << " (remaining: " << it->second.waitingConsumers.size() << ")" << std::endl;

                if (it->second.waitingConsumers.empty()) {
                    allAcked = true;
                }
            }
        }

        if (allAcked) {
            storage.remove(id);

            std::lock_guard<std::mutex> lock(ack_mtx);
            pendingAck.erase(id);

            std::cout << "Message " << id << " fully acknowledged, marked deleted" << std::endl;
        }

        metrics.consumed++;
        offsetManager.set(consumer, id);

        return "OK ack " + std::to_string(id);
    }

    return "ERROR unknown command";
}

void Server::replayHistory(const std::string& consumerName, int client_fd) {
    auto allSubs = subscriptionManager.getAll();

    uint64_t startOffset = offsetManager.get(consumerName);

    std::cout << "Replaying history for " << consumerName << " from offset " << startOffset << std::endl;

    uint64_t maxId = storage.getMaxId();

    int replayedCount = 0;

    for (uint64_t id = startOffset + 1; id <= maxId; id++) {
        std::string body = storage.readMessage(id);

        if (body.empty()) continue;

        std::string msgTopic = storage.getMessageTopic(id);

        if (msgTopic.empty()) continue;

        bool subscribed = false;
        for (auto& sub : allSubs) {
            if (sub.first == msgTopic) {
                for (auto& c : sub.second) {
                    if (c == consumerName) {
                        subscribed = true;
                        break;
                    }
                }
            }
            if (subscribed) break;
        }

        if (!subscribed) continue;

        std::string out = "MSG " + std::to_string(id) + " " + msgTopic + " " + body + "\n";
        write(client_fd, out.c_str(), out.size());
        replayedCount++;

        {
            std::lock_guard<std::mutex> lock(ack_mtx);
            if (pendingAck.find(id) != pendingAck.end()) {
                pendingAck[id].waitingConsumers.insert(consumerName);
            }
        }
    }

    if (replayedCount > 0) {
        std::cout << "Replayed " << replayedCount << " messages to " << consumerName << std::endl;
        sendResponse(client_fd, "SYS replay done " + std::to_string(replayedCount) + " messages");
    }
}

void Server::deliverMessage(const Message& msg) {
    auto consumers = subscriptionManager.get(msg.topic);

    if (consumers.empty()) return;

    std::set<std::string> waitingSet;
    for (auto& c : consumers) {
        waitingSet.insert(c);
    }

    {
        std::lock_guard<std::mutex> lock(ack_mtx);
        PendingMessage pm;
        pm.waitingConsumers = waitingSet;
        pm.topic = msg.topic;
        pm.lastRetry = std::chrono::steady_clock::now();
        pm.retryCount = 0;
        pendingAck[msg.id] = pm;
    }

    for (auto& consumer : consumers) {
        std::lock_guard<std::mutex> lock(conn_mtx);

        if (activeConsumers.find(consumer) != activeConsumers.end()) {
            int client = activeConsumers[consumer];

            std::string out = "MSG " + std::to_string(msg.id) + " " + msg.topic + " " + msg.body + "\n";
            write(client, out.c_str(), out.size());

            std::cout << "Delivered message " << msg.id << " to " << consumer << std::endl;
        } else {
            std::cout << "Consumer " << consumer << " offline, message " << msg.id << " pending" << std::endl;
        }
    }
}

void Server::consumerThread() {
    while (running) {
        Message msg = queue.pop();
        if (!running) break;

        uint64_t id = storage.store(msg.topic, msg.body);
        msg.id = id;

        deliverMessage(msg);

        metrics.stored++;
    }
}

void Server::retryThread() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!running) break;

        auto now = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(ack_mtx);

        std::vector<uint64_t> toRetry;

        for (auto& p : pendingAck) {
            if (p.second.waitingConsumers.empty()) continue;

            auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - p.second.lastRetry).count();

            if (diff > 5) {
                toRetry.push_back(p.first);
            }
        }

        for (auto& id : toRetry) {
            auto& pm = pendingAck[id];

            std::string msgBody = storage.readMessage(id);

            for (auto& consumer : pm.waitingConsumers) {
                std::lock_guard<std::mutex> connLock(conn_mtx);

                if (activeConsumers.find(consumer) != activeConsumers.end()) {
                    int client = activeConsumers[consumer];

                    std::string out = "MSG " + std::to_string(id) + " " + pm.topic + " " + msgBody + "\n";
                    write(client, out.c_str(), out.size());

                    std::cout << "Retry message " << id << " to " << consumer
                              << " (attempt " << pm.retryCount + 1 << ")" << std::endl;
                }
            }

            pm.lastRetry = now;
            pm.retryCount++;

            if (pm.retryCount > 10) {
                std::cout << "Message " << id << " exceeded max retries, dropping" << std::endl;
                storage.remove(id);
                pendingAck.erase(id);
            }
        }
    }
}

void Server::compactThread() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::minutes(10));
        if (!running) break;

        std::cout << "Starting periodic compaction..." << std::endl;
        storage.compact();
        std::cout << "Periodic compaction completed" << std::endl;
    }
}
