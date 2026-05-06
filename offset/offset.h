#ifndef OFFSET_H
#define OFFSET_H

#include <map>
#include <string>
#include <fstream>

class OffsetManager {
private:
    std::map<std::string, uint64_t> offsets;

public:
    void load() {
        std::ifstream file("offsets.dat");

        std::string consumer;
        uint64_t offset;

        while (file >> consumer >> offset) {
            offsets[consumer] = offset;
        }
    }

    void save() {
        std::ofstream file("offsets.dat");

        for (auto& p : offsets) {
            file << p.first << " " << p.second << "\n";
        }
    }

    void set(const std::string& consumer, uint64_t offset) {
        offsets[consumer] = offset;
        save();
    }

    uint64_t get(const std::string& consumer) {
        return offsets[consumer];
    }
};

#endif