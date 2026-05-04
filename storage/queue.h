#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <optional>

class SafeQueue {
private:
    std::queue<std::string> q;
    std::mutex mtx;
    std::condition_variable cv;
    bool stopped = false;

public:
    void push(const std::string& msg) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            q.push(msg);
        }
        cv.notify_one();
    }

    // Ждёт сообщение или возвращает std::nullopt, если очередь остановлена
    std::optional<std::string> pop() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !q.empty() || stopped; });

        if (q.empty() && stopped)
            return std::nullopt;

        std::string msg = std::move(q.front());
        q.pop();
        return msg;
    }

    // Пакетное извлечение (до maxCount сообщений)
    std::vector<std::string> popBatch(size_t maxCount) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !q.empty() || stopped; });

        std::vector<std::string> batch;
        while (batch.size() < maxCount && !q.empty()) {
            batch.push_back(std::move(q.front()));
            q.pop();
        }
        return batch;
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return q.size();
    }

    // Останавливает ожидание pop() — используется при завершении программы
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stopped = true;
        }
        cv.notify_all();
    }
};
