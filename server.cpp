#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

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

    offsetManager.load();
    subscriptionManager.load();

    std::thread(&Server::consumerThread, this).detach();
    std::thread(&Server::retryThread, this).detach();

    std::cout << "Server started on port " << port << std::endl;

    acceptClients();
}

void Server::acceptClients() {
    while (true) {
        int client = accept(server_fd, nullptr, nullptr);
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

    while (true) {
        int bytes = read(client_fd, temp, sizeof(temp) - 1);
        if (bytes <= 0) {
            if (!state.consumerName.empty()) {
                std::lock_guard<std::mutex> lock(conn_mtx);
                activeConsumers.erase(state.consumerName);
                std::cout << "Consumer disconnected: " << state.consumerName << std::endl;
            }
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

        subscriptionManager.add(topic, consumer);

        std::cout << consumer << " subscribed to " << topic << std::endl;
        return "OK subscribed to " + topic;
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
        }
        catch (...) {
            return "ERROR priority must be integer";
        }

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

        storage.remove(id);
        metrics.consumed++;

        offsetManager.set(consumer, id);

        std::lock_guard<std::mutex> lock(ack_mtx);
        pendingAck.erase(id);

        std::cout << "ACK " << id << " from " << consumer << std::endl;
        return "OK ack " + std::to_string(id);
    }

    return "ERROR unknown command";
}

void Server::consumerThread() {
    while (true) {
        Message msg = queue.pop();

        uint64_t id = storage.store(msg.body);
        msg.id = id;

        auto consumers = subscriptionManager.get(msg.topic);

        for (auto& consumer : consumers) {
            std::lock_guard<std::mutex> lock(conn_mtx);

            if (activeConsumers.find(consumer) != activeConsumers.end()) {
                int client = activeConsumers[consumer];

                std::string out = "MSG " + std::to_string(msg.id) + " " + msg.body + "\n";
                int res = write(client, out.c_str(), out.size());

                if (res > 0) {
                    std::lock_guard<std::mutex> lock2(ack_mtx);
                    pendingAck[msg.id] = std::chrono::steady_clock::now();
                }
            }
        }

        metrics.stored++;
    }
}

void Server::retryThread() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        auto now = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(ack_mtx);

        std::vector<uint64_t> toRetry;

        for (auto& p : pendingAck) {
            auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - p.second).count();

            if (diff > 5) {
                toRetry.push_back(p.first);
            }
        }

        for (auto& id : toRetry) {
            std::string msgBody = storage.readMessage(id);

            Message retryMsg;
            retryMsg.id = id;
            retryMsg.topic = "retry";
            retryMsg.body = msgBody;
            retryMsg.priority = 10;

            queue.push(retryMsg);

            std::cout << "Retry message " << id << std::endl;
        }
    }
}