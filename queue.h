#include "../message.h"
#include <queue>
#include <mutex>
#include <condition_variable>

struct Compare {
    bool operator()(const Message& a, const Message& b) {
        return a.priority < b.priority; // 🔥 выше priority → раньше
    }
};

class MessageQueue {
private:
    std::priority_queue<Message, std::vector<Message>, Compare> q;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void push(const Message& msg) {
        std::unique_lock<std::mutex> lock(mtx);
        q.push(msg);
        cv.notify_one();
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return q.size();
    }

    Message pop() {
        std::unique_lock<std::mutex> lock(mtx);

        while (q.empty()) {
            cv.wait(lock);
        }

        Message msg = q.top();
        q.pop();
        return msg;
    }
};