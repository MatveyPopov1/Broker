#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <cstdint> 

struct Message {
    uint64_t id;
    std::string topic;
    std::string body;
    int priority;
};

#endif