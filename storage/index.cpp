#include "storage/index.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

Index::Index() {
    fd = open("index.idx", O_CREAT | O_RDWR | O_APPEND, 0644);
    if (fd < 0) {
        perror("index open");
        exit(1);
    }
}

Index::~Index() {
    close(fd);
}

void Index::load() {
    lseek(fd, 0, SEEK_SET);

    IndexEntry entry;
    while (read(fd, &entry, sizeof(entry)) == sizeof(entry)) {
        map[entry.id] = entry;
    }
}

void Index::add(uint64_t id, uint64_t offset) {
    IndexEntry entry{ id, offset, 0 };

    write(fd, &entry, sizeof(entry));
    fsync(fd);

    map[id] = entry;
}

void Index::markDeleted(uint64_t id) {
    if (!exists(id)) return;

    map[id].deleted = 1;

    write(fd, &map[id], sizeof(IndexEntry)); // append çàïèñü
    fsync(fd);
}

bool Index::exists(uint64_t id) {
    return map.find(id) != map.end();
}

IndexEntry Index::get(uint64_t id) {
    return map[id];
}

std::unordered_map<uint64_t, IndexEntry>& Index::getAll() {
    return map;
}
