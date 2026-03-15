#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include "../traits.hpp"
#include "../../tools/traits.hpp"

namespace zeroclaw::hooks::builtin {
class CommandLoggerHook : public HookHandler {
public:
    CommandLoggerHook() = default;

    std::string name() const override {
        return "command-logger";
    }

    int32_t priority() const override {
        return -50;
    }

    void on_after_tool_call(const std::string& tool, const tools::ToolResult& result,
                           std::chrono::milliseconds duration) override {
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &now_time);
#else
        localtime_r(&now_time, &tm_buf);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%H:%M:%S");
        
        std::ostringstream entry;
        entry << "[" << ss.str() << "] " << tool << " (" << duration.count() << "ms) success=" << (result.success ? "true" : "false");
        
        std::lock_guard<std::mutex> lock(mutex_);
        log_.push_back(entry.str());
    }

    std::vector<std::string> entries() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return log_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::string> log_;
};
}
