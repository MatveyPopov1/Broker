#include "storage.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

Storage::Storage() {
    data_fd = open("data.log", O_CREAT | O_RDWR | O_APPEND, 0644);
    if (data_fd < 0) {
        perror("data open");
        exit(1);
    }

    index.load();

    currentId = 0;
    for (auto& p : index.getAll()) {
        if (p.first >= currentId)
            currentId = p.first + 1;
    }
}

Storage::~Storage() {
    close(data_fd);
}

uint64_t Storage::store(const std::string& text) {
    uint64_t id = currentId++;

    off_t offset = lseek(data_fd, 0, SEEK_END);

    MessageHeader header;
    header.size = text.size();
    header.id = id;
    header.type = 0;
    header.deleted = 0;

    write(data_fd, &header, sizeof(header));
    write(data_fd, text.c_str(), text.size());
    fsync(data_fd);

    index.add(id, offset);

    return id;
}

std::string Storage::readMessage(uint64_t id) {
    if (!index.exists(id))
        return "Нет сообщения";

    IndexEntry entry = index.get(id);

    if (entry.deleted)
        return "Сообщение удалено";

    lseek(data_fd, entry.offset, SEEK_SET);

    MessageHeader header;
    read(data_fd, &header, sizeof(header));

    char* buffer = new char[header.size + 1];
    read(data_fd, buffer, header.size);

    buffer[header.size] = '\0';

    std::string res(buffer);
    delete[] buffer;

    return res;
}

void Storage::remove(uint64_t id) {
    if (!index.exists(id)) return;

    index.markDeleted(id);
}

// 🔥 COMPACTION (ядро 10/10)
void Storage::compact() {
    int new_fd = open("data_new.log", O_CREAT | O_RDWR | O_TRUNC, 0644);

    if (new_fd < 0) {
        perror("compact open");
        return;
    }

    Index newIndex;

    for (auto& p : index.getAll()) {
        if (p.second.deleted) continue;

        lseek(data_fd, p.second.offset, SEEK_SET);

        MessageHeader header;
        read(data_fd, &header, sizeof(header));

        char* buffer = new char[header.size];
        read(data_fd, buffer, header.size);

        off_t newOffset = lseek(new_fd, 0, SEEK_END);

        write(new_fd, &header, sizeof(header));
        write(new_fd, buffer, header.size);

        newIndex.add(header.id, newOffset);

        delete[] buffer;
    }

    fsync(new_fd);

    close(data_fd);
    close(new_fd);

    rename("data_new.log", "data.log");

    data_fd = open("data.log", O_RDWR | O_APPEND);

    index = newIndex;
}