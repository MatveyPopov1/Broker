#ifndef METRICS_H
#define METRICS_H

#include <atomic>

struct Metrics {
    std::atomic<int> produced{ 0 };
    std::atomic<int> consumed{ 0 };
    std::atomic<int> stored{ 0 };
};

#endif