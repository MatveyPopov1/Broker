#pragma once

#ifndef STORAGE_H
#define STORAGE_H

#include <string>
#include <map>
#include "../index/index.h"

struct MessageHeader {
    uint32_t size;
    uint64_t id;
    uint8_t type;
    uint8_t deleted;
    uint32_t topicLength;
};

class Storage {
private:
    int data_fd;
    uint64_t currentId;

    Index index;

    std::map<uint64_t, std::string> topicIndex;

public:
    Storage();
    ~Storage();

    uint64_t store(const std::string& topic, const std::string& text);
    std::string readMessage(uint64_t id);
    std::string getMessageTopic(uint64_t id);
    uint64_t getMaxId();
    void remove(uint64_t id);

    void compact();
};

#endif
