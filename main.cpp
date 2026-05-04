#include "storage/storage.h"
#include <iostream>

int main() {
    Storage storage;

    int c;
    uint64_t id;
    std::string text;

    while (true) {
        std::cout << "\n1 Доюавить\n2 Прочитать\n3 Удалить\n4 Скомпановать\n0 Выход\n> ";
        std::cin >> c;
        std::cin.ignore();

        if (c == 1) {
            std::getline(std::cin, text);
            id = storage.store(text);
            std::cout << "ID: " << id << std::endl;
        }
        else if (c == 2) {
            std::cin >> id;
            std::cout << storage.readMessage(id) << std::endl;
        }
        else if (c == 3) {
            std::cin >> id;
            storage.remove(id);
        }
        else if (c == 4) {
            storage.compact();
            std::cout << "Уплотнено!\n";
        }
        else break;
    }
}
