#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <sstream>
#include <algorithm>

void Server::start(int port) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

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

void Server::handleClient(int client_fd) {
    char buffer[1024];
    std::string consumerName;

    while (true) {
        int bytes = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) break;

        buffer[bytes] = '\0';
        std::string input(buffer);

        // CONNECT consumer1
        if (input.rfind("CONNECT", 0) == 0) {
            std::istringstream iss(input);
            std::string cmd;
            iss >> cmd >> consumerName;

            std::lock_guard<std::mutex> lock(conn_mtx);
            activeConsumers[consumerName] = client_fd;

            std::cout << "Connected consumer: " << consumerName << std::endl;
        }

        // SUB topic consumer
        else if (input.rfind("SUB", 0) == 0) {
            std::istringstream iss(input);
            std::string cmd, topic, consumer;
            iss >> cmd >> topic >> consumer;

            subscriptionManager.add(topic, consumer);

            std::cout << consumer << " subscribed to " << topic << std::endl;
        }

        // PUB topic message priority
        else if (input.rfind("PUB", 0) == 0) {
            std::istringstream iss(input);

            std::string cmd, topic;
            iss >> cmd >> topic;

            std::string rest;
            std::getline(iss, rest);

            rest.erase(std::remove(rest.begin(), rest.end(), '\n'), rest.end());
            rest.erase(std::remove(rest.begin(), rest.end(), '\r'), rest.end());

            size_t lastSpace = rest.find_last_of(' ');

            std::string body = rest.substr(0, lastSpace);
            int priority = std::stoi(rest.substr(lastSpace + 1));

            Message msg;
            msg.topic = topic;
            msg.body = body;
            msg.priority = priority;

            queue.push(msg);
            metrics.produced++;
        }

        // ACK id consumer
        else if (input.rfind("ACK", 0) == 0) {
            std::istringstream iss(input);

            std::string cmd, consumer;
            uint64_t id;

            iss >> cmd >> id >> consumer;

            storage.remove(id);
            metrics.consumed++;

            offsetManager.set(consumer, id);

            std::lock_guard<std::mutex> lock(ack_mtx);
            pendingAck.erase(id);

            std::cout << "ACK " << id << std::endl;
        }
    }

    close(client_fd);
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

        for (auto& p : pendingAck) {
            auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - p.second).count();

            if (diff > 5) {
                std::string msg = storage.readMessage(p.first);

                Message retryMsg;
                retryMsg.id = p.first;
                retryMsg.topic = "retry";
                retryMsg.body = msg;
                retryMsg.priority = 10;

                queue.push(retryMsg);

                std::cout << "Retry " << p.first << std::endl;
            }
        }
    }
}