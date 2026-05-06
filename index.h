#pragma once

#ifndef INDEX_H
#define INDEX_H

#include <unordered_map>
#include <cstdint>

struct IndexEntry {
    uint64_t id;
    uint64_t offset;
    uint8_t deleted;
};

class Index {
private:
    int fd;
    std::unordered_map<uint64_t, IndexEntry> map;

public:
    Index();
    ~Index();

    void load();
    void add(uint64_t id, uint64_t offset);
    void markDeleted(uint64_t id);

    bool exists(uint64_t id);
    IndexEntry get(uint64_t id);

    std::unordered_map<uint64_t, IndexEntry>& getAll();
};

#endif