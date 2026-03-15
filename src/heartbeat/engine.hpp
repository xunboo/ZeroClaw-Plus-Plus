#pragma once

#include "../observability/observer.hpp"
#include "../config/config.hpp"
#include <filesystem>
#include <memory>
#include <vector>
#include <string>
#include <future>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace zeroclaw::heartbeat {

class HeartbeatEngine {
public:
    using TickCallback = std::function<void(size_t)>;

    HeartbeatEngine(config::HeartbeatConfig config,
                    std::filesystem::path workspace_dir,
                    std::shared_ptr<observability::Observer> observer);

    ~HeartbeatEngine();

    std::future<void> run();
    void stop();

    std::vector<std::string> collect_tasks() const;
    static std::vector<std::string> parse_tasks(const std::string& content);
    static bool ensure_heartbeat_file(const std::filesystem::path& workspace_dir);

private:
    size_t tick() const;
    void run_loop();

    config::HeartbeatConfig config_;
    std::filesystem::path workspace_dir_;
    std::shared_ptr<observability::Observer> observer_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

}
