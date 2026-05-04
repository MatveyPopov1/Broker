#pragma once
#include <atomic>
#include <iostream>

struct Metrics {
    std::atomic<uint64_t> produced{0};
    std::atomic<uint64_t> stored{0};

    void print() const {
        std::cout << "Produced: " << produced
                  << " Stored: " << stored << std::endl;
    }
};
