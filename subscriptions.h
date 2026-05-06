#ifndef SUBSCRIPTIONS_H
#define SUBSCRIPTIONS_H

#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <mutex>
#include <set>
#include <algorithm>

class SubscriptionManager {
private:
    std::map<std::string, std::vector<std::string>> subs;
    std::set<std::string> knownTopics;
    std::mutex mtx;

    void saveSubscriptions() {
        std::ofstream file("subscriptions.dat");

        for (auto& p : subs) {
            for (auto& c : p.second) {
                file << p.first << " " << c << "\n";
            }
        }
    }

    void saveTopics() {
        std::ofstream file("topics.dat");

        for (auto& t : knownTopics) {
            file << t << "\n";
        }
    }

    void loadTopics() {
        std::ifstream file("topics.dat");

        std::string topic;
        while (file >> topic) {
            knownTopics.insert(topic);
        }
    }

public:
    void load() {
        std::lock_guard<std::mutex> lock(mtx);

        std::ifstream subFile("subscriptions.dat");

        std::string topic, consumer;
        while (subFile >> topic >> consumer) {
            subs[topic].push_back(consumer);
            knownTopics.insert(topic);
        }

        loadTopics();
    }

    void ensureTopic(const std::string& topic) {
        std::lock_guard<std::mutex> lock(mtx);

        knownTopics.insert(topic);
        saveTopics();
    }

    bool add(const std::string& topic, const std::string& consumer) {
        std::lock_guard<std::mutex> lock(mtx);

        for (auto& c : subs[topic]) {
            if (c == consumer) {
                return false;
            }
        }

        subs[topic].push_back(consumer);
        knownTopics.insert(topic);

        saveSubscriptions();
        saveTopics();

        return true;
    }

    bool remove(const std::string& topic, const std::string& consumer) {
        std::lock_guard<std::mutex> lock(mtx);

        auto it = subs.find(topic);
        if (it == subs.end()) {
            return false;
        }

        auto& consumers = it->second;
        auto pos = std::find(consumers.begin(), consumers.end(), consumer);
        if (pos == consumers.end()) {
            return false;
        }

        consumers.erase(pos);

        if (consumers.empty()) {
            subs.erase(it);
        }

        saveSubscriptions();

        return true;
    }

    bool topicExists(const std::string& topic) {
        std::lock_guard<std::mutex> lock(mtx);
        return knownTopics.find(topic) != knownTopics.end();
    }

    std::vector<std::string> get(const std::string& topic) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = subs.find(topic);
        if (it != subs.end()) {
            return it->second;
        }
        return std::vector<std::string>();
    }

    std::map<std::string, std::vector<std::string>> getAll() {
        std::lock_guard<std::mutex> lock(mtx);
        return subs;
    }

    std::vector<std::string> getTopics() {
        std::lock_guard<std::mutex> lock(mtx);

        std::vector<std::string> topics;
        for (auto& t : knownTopics) {
            topics.push_back(t);
        }
        return topics;
    }

    std::vector<std::string> getActiveTopics() {
        std::lock_guard<std::mutex> lock(mtx);

        std::vector<std::string> topics;
        for (auto& p : subs) {
            topics.push_back(p.first);
        }
        return topics;
    }

    std::vector<std::string> getSubscriptions(const std::string& consumer) {
        std::lock_guard<std::mutex> lock(mtx);

        std::vector<std::string> result;
        for (auto& p : subs) {
            for (auto& c : p.second) {
                if (c == consumer) {
                    result.push_back(p.first);
                    break;
                }
            }
        }
        return result;
    }
};

#endif
