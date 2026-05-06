#include "doctest.h"
#include "../subscriptions/subscriptions.h"
#include <cstdio>
#include <thread>

TEST_CASE("SubscriptionManager basic operations") {
    std::remove("subscriptions.dat");
    std::remove("topics.dat");

    SubscriptionManager sm;

    SUBCASE("Add subscription") {
        bool added = sm.add("sensors", "alex");
        CHECK(added == true);
    }

    SUBCASE("Duplicate subscription returns false") {
        sm.add("sensors", "alex");
        bool added = sm.add("sensors", "alex");
        CHECK(added == false);
    }

    SUBCASE("Get subscribers for topic") {
        sm.add("sensors", "alex");
        sm.add("sensors", "boris");
        auto subs = sm.get("sensors");
        CHECK(subs.size() == 2);
    }

    SUBCASE("Get subscribers for non-existent topic returns empty") {
        auto subs = sm.get("nonexistent");
        CHECK(subs.empty());
    }

    SUBCASE("Remove subscription") {
        sm.add("sensors", "alex");
        bool removed = sm.remove("sensors", "alex");
        CHECK(removed == true);
        auto subs = sm.get("sensors");
        CHECK(subs.empty());
    }

    SUBCASE("Remove non-existent subscription returns false") {
        bool removed = sm.remove("sensors", "nobody");
        CHECK(removed == false);
    }

    SUBCASE("Remove last subscriber removes topic from active") {
        sm.add("sensors", "alex");
        sm.remove("sensors", "alex");
        auto active = sm.getActiveTopics();
        CHECK(active.empty());
    }

    SUBCASE("Remove one subscriber keeps topic active if others remain") {
        sm.add("sensors", "alex");
        sm.add("sensors", "boris");
        sm.remove("sensors", "alex");
        auto active = sm.getActiveTopics();
        CHECK(active.size() == 1);
        CHECK(active[0] == "sensors");
    }

    SUBCASE("GetTopics returns all known topics") {
        sm.add("sensors", "alex");
        sm.ensureTopic("alarms");
        auto topics = sm.getTopics();
        CHECK(topics.size() == 2);
    }

    SUBCASE("GetActiveTopics returns only topics with subscribers") {
        sm.add("sensors", "alex");
        sm.ensureTopic("alarms");
        auto active = sm.getActiveTopics();
        CHECK(active.size() == 1);
        CHECK(active[0] == "sensors");
    }

    SUBCASE("GetSubscriptions for consumer") {
        sm.add("sensors", "alex");
        sm.add("temperature", "alex");
        sm.add("humidity", "boris");
        auto subs = sm.getSubscriptions("alex");
        CHECK(subs.size() == 2);
    }

    SUBCASE("GetSubscriptions for consumer with no subs") {
        auto subs = sm.getSubscriptions("nobody");
        CHECK(subs.empty());
    }

    SUBCASE("GetAll returns full map") {
        sm.add("sensors", "alex");
        sm.add("temperature", "boris");
        auto all = sm.getAll();
        CHECK(all.size() == 2);
        CHECK(all["sensors"].size() == 1);
        CHECK(all["temperature"].size() == 1);
    }

    SUBCASE("Topic exists check") {
        sm.add("sensors", "alex");
        CHECK(sm.topicExists("sensors") == true);
        CHECK(sm.topicExists("nonexistent") == false);
    }

    SUBCASE("Load from file restores subscriptions and topics") {
        sm.add("sensors", "alex");
        sm.add("temperature", "boris");
        sm.ensureTopic("alarms");

        SubscriptionManager sm2;
        sm2.load();

        auto topics = sm2.getTopics();
        CHECK(topics.size() == 3);

        auto active = sm2.getActiveTopics();
        CHECK(active.size() == 2);

        auto subs = sm2.get("sensors");
        CHECK(subs.size() == 1);
        CHECK(subs[0] == "alex");
    }

    std::remove("subscriptions.dat");
    std::remove("topics.dat");
}

TEST_CASE("SubscriptionManager thread safety") {
    std::remove("subscriptions.dat");
    std::remove("topics.dat");

    SubscriptionManager sm;

    std::thread t1([&sm]() {
        for (int i = 0; i < 100; i++) {
            sm.add("topic_a", "user_" + std::to_string(i));
        }
    });

    std::thread t2([&sm]() {
        for (int i = 0; i < 100; i++) {
            sm.add("topic_b", "user_" + std::to_string(i));
        }
    });

    std::thread t3([&sm]() {
        for (int i = 0; i < 50; i++) {
            sm.get("topic_a");
            sm.getTopics();
            sm.getActiveTopics();
        }
    });

    t1.join();
    t2.join();
    t3.join();

    auto subsA = sm.get("topic_a");
    auto subsB = sm.get("topic_b");
    CHECK(subsA.size() == 100);
    CHECK(subsB.size() == 100);

    std::remove("subscriptions.dat");
    std::remove("topics.dat");
}
