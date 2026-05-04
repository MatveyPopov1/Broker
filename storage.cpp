#include "storage.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

Storage::Storage() {
    // Открываем data.log
    data_fd = open("data.log", O_CREAT | O_RDWR | O_APPEND, 0644);
    if (data_fd < 0) {
        perror("Ошибка открытия data.log");
        exit(1);
    }

    // Открываем index.idx
    index_fd = open("index.idx", O_CREAT | O_RDWR | O_APPEND, 0644);
    if (index_fd < 0) {
        perror("Ошибка открытия index.idx");
        exit(1);
    }

    currentId = 0;

    loadIndex();
}

Storage::~Storage() {
    close(data_fd);
    close(index_fd);
}

// 🔥 Загрузка индекса в память
void Storage::loadIndex() {
    lseek(index_fd, 0, SEEK_SET);

    IndexEntry entry;
    while (read(index_fd, &entry, sizeof(entry)) == sizeof(entry)) {
        indexMap[entry.id] = entry.offset;
        if (entry.id >= currentId) {
            currentId = entry.id + 1;
        }
    }
}

// 🔥 Запись сообщения
uint64_t Storage::storeMessage(const std::string& text) {
    uint64_t id = currentId++;

    // Получаем текущую позицию
    off_t offset = lseek(data_fd, 0, SEEK_END);

    MessageHeader header;
    header.size = text.size();
    header.id = id;
    header.type = 0;

    // 1. Пишем заголовок
    write(data_fd, &header, sizeof(header));

    // 2. Пишем текст
    write(data_fd, text.c_str(), text.size());

    // 3. Гарантируем запись
    fsync(data_fd);

    // 4. Записываем индекс
    IndexEntry entry;
    entry.id = id;
    entry.offset = offset;

    write(index_fd, &entry, sizeof(entry));
    fsync(index_fd);

    // 5. В память
    indexMap[id] = offset;

    return id;
}

// 🔥 Чтение по индексу
std::string Storage::readMessage(uint64_t id) {
    if (indexMap.find(id) == indexMap.end()) {
        return "Сообщение не найдено";
    }

    uint64_t offset = indexMap[id];

    // Переходим к позиции
    lseek(data_fd, offset, SEEK_SET);

    MessageHeader header;
    read(data_fd, &header, sizeof(header));

    char* buffer = new char[header.size + 1];
    read(data_fd, buffer, header.size);

    buffer[header.size] = '\0';

    std::string result(buffer);
    delete[] buffer;

    return result;
}