#pragma once

#ifndef STORAGE_H
#define STORAGE_H

#include <string>
#include "../index/index.h"

struct MessageHeader {
    uint32_t size;
    uint64_t id;
    uint8_t type;
    uint8_t deleted;
};

class Storage {
private:
    int data_fd;
    uint64_t currentId;

    Index index;

public:
    Storage();
    ~Storage();

    uint64_t store(const std::string& text);
    std::string readMessage(uint64_t id);
    void remove(uint64_t id);

    void compact();
};

#endif
