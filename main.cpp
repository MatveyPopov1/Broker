#include "storage.h"
#include <iostream>

int main() {
    Storage storage;

    int choice;
    std::string text;
    uint64_t id;

    while (true) {
        std::cout << "\n1. Добавить сообщение\n";
        std::cout << "2. Получить сообщение по ID\n";
        std::cout << "0. Выход\n> ";

        std::cin >> choice;
        std::cin.ignore();

        if (choice == 1) {
            std::cout << "Введите текст: ";
            std::getline(std::cin, text);

            uint64_t id = storage.storeMessage(text);
            std::cout << "Сохранено с ID: " << id << std::endl;
        }
        else if (choice == 2) {
            std::cout << "Введите ID: ";
            std::cin >> id;

            std::string msg = storage.readMessage(id);
            std::cout << "Сообщение: " << msg << std::endl;
        }
        else {
            break;
        }
    }

    return 0;
}