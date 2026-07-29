#pragma once

#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace overlay {
    enum class LogLevel {
        Info,
        Warning,
        Error
    };

    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        LogLevel level = LogLevel::Info;
        std::string message;
    };

    class Logger {
    public:
        explicit Logger(size_t capacity = 256) : m_capacity(capacity) {}

        void Write(LogLevel level, std::string_view message);
        [[nodiscard]] std::vector<LogEntry> Snapshot() const;
        void Clear();

    private:
        size_t m_capacity;
        mutable std::mutex m_mutex;
        std::deque<LogEntry> m_entries;
    };
}
