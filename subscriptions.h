#ifndef SUBSCRIPTIONS_H
#define SUBSCRIPTIONS_H

#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <mutex>

class SubscriptionManager {
private:
    std::map<std::string, std::vector<std::string>> subs;
    std::mutex mtx;

public:
    void load() {
        std::lock_guard<std::mutex> lock(mtx);

        std::ifstream file("subscriptions.dat");

        std::string topic, consumer;

        while (file >> topic >> consumer) {
            subs[topic].push_back(consumer);
        }
    }

    void save() {
        std::lock_guard<std::mutex> lock(mtx);

        std::ofstream file("subscriptions.dat");

        for (auto& p : subs) {
            for (auto& c : p.second) {
                file << p.first << " " << c << "\n";
            }
        }
    }

    void add(const std::string& topic, const std::string& consumer) {
        std::lock_guard<std::mutex> lock(mtx);

        for (auto& c : subs[topic]) {
            if (c == consumer) {
                return;
            }
        }

        subs[topic].push_back(consumer);

        std::ofstream file("subscriptions.dat");

        for (auto& p : subs) {
            for (auto& c : p.second) {
                file << p.first << " " << c << "\n";
            }
        }
    }

    std::vector<std::string> get(const std::string& topic) {
        std::lock_guard<std::mutex> lock(mtx);
        return subs[topic];
    }

    std::map<std::string, std::vector<std::string>> getAll() {
        std::lock_guard<std::mutex> lock(mtx);
        return subs;
    }
};

#endif
