#include "engine.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace zeroclaw::heartbeat {

HeartbeatEngine::HeartbeatEngine(config::HeartbeatConfig config,
                                 std::filesystem::path workspace_dir,
                                 std::shared_ptr<observability::Observer> observer)
    : config_(std::move(config))
    , workspace_dir_(std::move(workspace_dir))
    , observer_(std::move(observer)) {}

HeartbeatEngine::~HeartbeatEngine() {
    stop();
}

std::future<void> HeartbeatEngine::run() {
    if (!config_.enabled) {
        std::promise<void> p;
        p.set_value();
        return p.get_future();
    }

    running_.store(true);
    stopped_.store(false);

    std::promise<void> started_promise;
    auto started_future = started_promise.get_future();

    thread_ = std::thread([this, p = std::move(started_promise)]() mutable {
        p.set_value();
        run_loop();
    });

    return started_future;
}

void HeartbeatEngine::stop() {
    stopped_.store(true);
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
    running_.store(false);
}

void HeartbeatEngine::run_loop() {
    auto interval_mins = std::max(config_.interval_minutes, 30u);
    auto interval = std::chrono::minutes(interval_mins);

    while (!stopped_.load()) {
        observer_->record_event(observability::HeartbeatTickEvent{});

        try {
            auto tasks = tick();
            if (tasks > 0) {
            }
        } catch (const std::exception& e) {
            observability::ErrorEvent err;
            err.component = "heartbeat";
            err.message = e.what();
            observer_->record_event(err);
        }

        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, interval, [this] { return stopped_.load(); });
    }
}

size_t HeartbeatEngine::tick() const {
    return collect_tasks().size();
}

std::vector<std::string> HeartbeatEngine::collect_tasks() const {
    auto heartbeat_path = workspace_dir_ / "HEARTBEAT.md";
    if (!std::filesystem::exists(heartbeat_path)) {
        return {};
    }

    std::ifstream file(heartbeat_path);
    if (!file) {
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_tasks(buffer.str());
}

std::vector<std::string> HeartbeatEngine::parse_tasks(const std::string& content) {
    std::vector<std::string> tasks;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) {
            continue;
        }

        auto trimmed = line.substr(start);
        if (trimmed.size() >= 2 && trimmed[0] == '-' && trimmed[1] == ' ') {
            auto task = trimmed.substr(2);
            auto end = task.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) {
                task = task.substr(0, end + 1);
            }
            if (!task.empty()) {
                tasks.push_back(task);
            }
        }
    }

    return tasks;
}

bool HeartbeatEngine::ensure_heartbeat_file(const std::filesystem::path& workspace_dir) {
    auto path = workspace_dir / "HEARTBEAT.md";
    if (std::filesystem::exists(path)) {
        return true;
    }

    std::ofstream file(path);
    if (!file) {
        return false;
    }

    file << "# Periodic Tasks\n\n"
         << "# Add tasks below (one per line, starting with `- `)\n"
         << "# The agent will check this file on each heartbeat tick.\n"
         << "#\n"
         << "# Examples:\n"
         << "# - Check my email for important messages\n"
         << "# - Review my calendar for upcoming events\n"
         << "# - Check the weather forecast\n";

    return true;
}

}
