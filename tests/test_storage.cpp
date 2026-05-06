#include "doctest.h"
#include "../storage/storage.h"
#include <cstdio>

TEST_CASE("Storage basic operations") {
    std::remove("data.log");
    std::remove("index.idx");
    std::remove("data_new.log");

    SUBCASE("Store and read message") {
        Storage storage;
        uint64_t id = storage.store("test_topic", "Hello world");
        std::string body = storage.readMessage(id);
        CHECK(body == "Hello world");
    }

    SUBCASE("Read non-existent message returns empty") {
        Storage storage;
        std::string body = storage.readMessage(9999);
        CHECK(body.empty());
    }

    SUBCASE("Get topic by id") {
        Storage storage;
        uint64_t id = storage.store("sensors", "temperature=25");
        std::string topic = storage.getMessageTopic(id);
        CHECK(topic == "sensors");
    }

    SUBCASE("Remove message") {
        Storage storage;
        uint64_t id = storage.store("test", "data");
        storage.remove(id);
        std::string body = storage.readMessage(id);
        CHECK(body.empty());
    }

    SUBCASE("Max ID increments correctly") {
        Storage storage;
        uint64_t id1 = storage.store("test", "msg1");
        uint64_t id2 = storage.store("test", "msg2");
        CHECK(id2 == id1 + 1);
    }

    std::remove("data.log");
    std::remove("index.idx");
    std::remove("data_new.log");
}
