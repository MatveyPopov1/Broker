#pragma once

#ifndef STORAGE_H
#define STORAGE_H

#include <cstdint>
#include <string>
#include <unordered_map>

struct MessageHeader {
    uint32_t size;   // размер текста
    uint64_t id;     // ID сообщения
    uint8_t type;    // тип (пока не используем)
};

struct IndexEntry {
    uint64_t id;
    uint64_t offset;
};

class Storage {
private:
    int data_fd;
    int index_fd;

    uint64_t currentId;

    std::unordered_map<uint64_t, uint64_t> indexMap;

    void loadIndex();

public:
    Storage();
    ~Storage();

    uint64_t storeMessage(const std::string& text);
    std::string readMessage(uint64_t id);
};

#endif