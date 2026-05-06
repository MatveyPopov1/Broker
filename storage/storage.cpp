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

uint64_t Storage::store(const std::string& topic, const std::string& text) {
    uint64_t id = currentId++;

    off_t offset = lseek(data_fd, 0, SEEK_END);

    MessageHeader header;
    header.size = text.size();
    header.id = id;
    header.type = 0;
    header.deleted = 0;
    header.topicLength = topic.size();

    write(data_fd, &header, sizeof(header));
    write(data_fd, topic.c_str(), topic.size());
    write(data_fd, text.c_str(), text.size());
    fsync(data_fd);

    index.add(id, offset);
    topicIndex[id] = topic;

    return id;
}

std::string Storage::readMessage(uint64_t id) {
    if (!index.exists(id))
        return "";

    IndexEntry entry = index.get(id);

    if (entry.deleted)
        return "";

    lseek(data_fd, entry.offset, SEEK_SET);

    MessageHeader header;
    read(data_fd, &header, sizeof(header));

    lseek(data_fd, header.topicLength, SEEK_CUR);

    char* buffer = new char[header.size + 1];
    read(data_fd, buffer, header.size);

    buffer[header.size] = '\0';

    std::string res(buffer);
    delete[] buffer;

    return res;
}

std::string Storage::getMessageTopic(uint64_t id) {
    auto it = topicIndex.find(id);
    if (it != topicIndex.end()) {
        return it->second;
    }

    if (!index.exists(id))
        return "";

    IndexEntry entry = index.get(id);

    if (entry.deleted)
        return "";

    lseek(data_fd, entry.offset, SEEK_SET);

    MessageHeader header;
    read(data_fd, &header, sizeof(header));

    char* buffer = new char[header.topicLength + 1];
    read(data_fd, buffer, header.topicLength);

    buffer[header.topicLength] = '\0';

    std::string topic(buffer);
    delete[] buffer;

    topicIndex[id] = topic;

    return topic;
}

uint64_t Storage::getMaxId() {
    if (currentId == 0) return 0;
    return currentId - 1;
}

void Storage::remove(uint64_t id) {
    if (!index.exists(id)) return;

    index.markDeleted(id);
}

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

        char* topicBuffer = new char[header.topicLength];
        read(data_fd, topicBuffer, header.topicLength);

        char* bodyBuffer = new char[header.size];
        read(data_fd, bodyBuffer, header.size);

        off_t newOffset = lseek(new_fd, 0, SEEK_END);

        write(new_fd, &header, sizeof(header));
        write(new_fd, topicBuffer, header.topicLength);
        write(new_fd, bodyBuffer, header.size);

        newIndex.add(header.id, newOffset);

        delete[] topicBuffer;
        delete[] bodyBuffer;
    }

    fsync(new_fd);

    close(data_fd);
    close(new_fd);

    rename("data_new.log", "data.log");

    data_fd = open("data.log", O_RDWR | O_APPEND);

    index = newIndex;
}
