#include "overlay/Logger.hpp"

namespace overlay {
    void Logger::Write(LogLevel level, std::string_view message) {
        std::lock_guard lock(m_mutex);
        m_entries.push_back({
            std::chrono::system_clock::now(),
            level,
            std::string(message)
        });
        while (m_entries.size() > m_capacity) {
            m_entries.pop_front();
        }
    }

    std::vector<LogEntry> Logger::Snapshot() const {
        std::lock_guard lock(m_mutex);
        return {m_entries.begin(), m_entries.end()};
    }

    void Logger::Clear() {
        std::lock_guard lock(m_mutex);
        m_entries.clear();
    }
}
