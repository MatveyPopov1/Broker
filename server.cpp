#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <sstream>
#include <algorithm>

std::mutex sub_mtx;
std::map<std::string, std::vector<int>> subscribers;

void Server::start(int port) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    std::thread(&Server::consumerThread, this).detach();

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

    while (true) {
        int bytes = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) break;

        buffer[bytes] = '\0';

        std::string input(buffer);



        if (input.rfind("SUB", 0) == 0) {
            std::string topic = input.substr(4);

            // 🔥 УДАЛЯЕМ \n и \r
            topic.erase(std::remove(topic.begin(), topic.end(), '\n'), topic.end());
            topic.erase(std::remove(topic.begin(), topic.end(), '\r'), topic.end());

            std::lock_guard<std::mutex> lock(sub_mtx);
            subscribers[topic].push_back(client_fd);

            std::cout << "Client subscribed to [" << topic << "]" << std::endl;

            std::cout << "Subscribers for [" << topic << "]: "
                << subscribers[topic].size() << std::endl;
        }
        else if (input.rfind("ACK", 0) == 0) {
            uint64_t id = std::stoull(input.substr(4));

            storage.remove(id); // 🔥 помечаем как обработанное
            metrics.consumed++;

            std::cout << "ACK received for " << id << std::endl;
        }
        else if (input.rfind("PUB", 0) == 0) {
            std::istringstream iss(input);

            std::string cmd, topic;
            iss >> cmd >> topic;

            std::string rest;
            std::getline(iss, rest);

            // 🔥 убираем \n
            rest.erase(std::remove(rest.begin(), rest.end(), '\n'), rest.end());
            rest.erase(std::remove(rest.begin(), rest.end(), '\r'), rest.end());

            size_t lastSpace = rest.find_last_of(' ');

            if (lastSpace == std::string::npos) {
                std::cout << "Invalid PUB format\n";
                continue;
            }

            std::string body = rest.substr(0, lastSpace);
            int priority = std::stoi(rest.substr(lastSpace + 1));

            Message msg;
            msg.topic = topic;
            msg.body = body;
            msg.priority = priority;

            queue.push(msg);
            metrics.produced++;

            std::cout << "Received PUB: [" << topic << "] " << body << std::endl;
        }
    }

    close(client_fd);
}
void Server::consumerThread() {
    while (true) {
        if (queue.size() > 1000) {
            Message msg = queue.pop();
            std::string body = msg.body; // если нужен текст
            storage.store(body);
            metrics.stored++;
        }

        Message msg = queue.pop();

        uint64_t id = storage.store(msg.body);
        msg.id = id;

        std::lock_guard<std::mutex> lock(sub_mtx);

        std::cout << "Looking for subscribers of topic: [" << msg.topic << "]\n";
        if (subscribers.find(msg.topic) != subscribers.end()) {
            for (int client : subscribers[msg.topic]) {

                std::string out = "MSG " + std::to_string(msg.id) + " " + msg.body + "\n";
                int res = write(client, out.c_str(), out.size());

                if (res <= 0) {
                    std::cout << "❌ Failed to send to client\n";
                }
                else {
                    std::cout << "✅ Sent to client: " << out;
                }
            }
        }

        metrics.stored++;

        std::cout << "Produced: " << metrics.produced
            << " Stored: " << metrics.stored << std::endl;
    }
}
