#pragma once
#include <fstream>
#include <mutex>
#include <string>

enum class LogLevel { INFO, WARN, ERROR };

class Logger {
public:
    explicit Logger(const std::string& filePath);
    void log(LogLevel level, const std::string& message);

private:
    std::ofstream file_;
    std::mutex mutex_;
};