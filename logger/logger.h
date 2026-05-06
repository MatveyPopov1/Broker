#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
private:
    std::ofstream logFile;
    std::mutex mtx;
    bool consoleOutput;
    LogLevel minLevel;

    std::string levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::INFO:    return "INFO";
            case LogLevel::WARNING: return "WARN";
            case LogLevel::ERROR:   return "ERROR";
        }
        return "UNKNOWN";
    }

    std::string getTimestamp() {
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

public:
    Logger() : consoleOutput(true), minLevel(LogLevel::DEBUG) {}

    bool init(const std::string& filename) {
        logFile.open(filename, std::ios::out | std::ios::app);
        return logFile.is_open();
    }

    void setConsoleOutput(bool enabled) {
        consoleOutput = enabled;
    }

    void setMinLevel(LogLevel level) {
        minLevel = level;
    }

    void log(LogLevel level, const std::string& message) {
        if (level < minLevel) return;

        std::lock_guard<std::mutex> lock(mtx);

        std::string timestamp = getTimestamp();
        std::string levelStr = levelToString(level);
        std::string line = "[" + timestamp + "] [" + levelStr + "] " + message;

        if (logFile.is_open()) {
            logFile << line << std::endl;
            logFile.flush();
        }

        if (consoleOutput) {
            if (level == LogLevel::ERROR) {
                std::cerr << line << std::endl;
            } else {
                std::cout << line << std::endl;
            }
        }
    }

    void debug(const std::string& message)   { log(LogLevel::DEBUG, message); }
    void info(const std::string& message)    { log(LogLevel::INFO, message); }
    void warning(const std::string& message) { log(LogLevel::WARNING, message); }
    void error(const std::string& message)   { log(LogLevel::ERROR, message); }

    ~Logger() {
        if (logFile.is_open()) {
            logFile.close();
        }
    }
};

#endif
