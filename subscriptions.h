#ifndef SUBSCRIPTIONS_H
#define SUBSCRIPTIONS_H

#include <map>
#include <vector>
#include <string>
#include <fstream>

class SubscriptionManager {
private:
    std::map<std::string, std::vector<std::string>> subs;

public:
    void load() {
        std::ifstream file("subscriptions.dat");

        std::string topic, consumer;

        while (file >> topic >> consumer) {
            subs[topic].push_back(consumer);
        }
    }

    void save() {
        std::ofstream file("subscriptions.dat");

        for (auto& p : subs) {
            for (auto& c : p.second) {
                file << p.first << " " << c << "\n";
            }
        }
    }

    void add(const std::string& topic, const std::string& consumer) {
        subs[topic].push_back(consumer);
        save();
    }

    std::vector<std::string> get(const std::string& topic) {
        return subs[topic];
    }
};

#endif