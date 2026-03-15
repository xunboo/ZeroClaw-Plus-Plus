#pragma once

#include <string>
#include <memory>
#include <functional>
#include <future>
#include <chrono>
#include <vector>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <optional>

namespace zeroclaw {
namespace config {
    class Config;
}



namespace daemon {
    constexpr uint64_t STATUS_FLUSH_SECONDS = 5;

    struct DaemonResult {
        bool success;
        std::string error;
    };

    std::filesystem::path state_file_path(const config::Config& config);

    class ComponentSupervisor {
    public:
        using ComponentFunc = std::function<std::future<DaemonResult>()>;
        
        ComponentSupervisor(const std::string& name,
                           uint64_t initial_backoff_secs,
                           uint64_t max_backoff_secs,
                           ComponentFunc run_component);
        ~ComponentSupervisor();
        
        void start();
        void stop();
        void abort();
        
    private:
        void supervisor_loop();
        
        std::string name_;
        uint64_t initial_backoff_secs_;
        uint64_t max_backoff_secs_;
        ComponentFunc run_component_;
        std::atomic<bool> running_{false};
        std::atomic<bool> aborted_{false};
        std::thread thread_;
        std::mutex mutex_;
        std::condition_variable cv_;
    };

    class StateWriter {
    public:
        StateWriter(std::shared_ptr<config::Config> config);
        ~StateWriter();
        
        void start();
        void stop();
        
    private:
        void writer_loop();
        
        std::shared_ptr<config::Config> config_;
        std::filesystem::path path_;
        std::atomic<bool> running_{false};
        std::thread thread_;
        std::mutex mutex_;
        std::condition_variable cv_;
    };

    class DaemonRunner {
    public:
        DaemonRunner();
        ~DaemonRunner();
        
        std::future<DaemonResult> run(std::shared_ptr<config::Config> config, std::string host, uint16_t port);
        void shutdown();
        
    private:
        void run_internal(std::shared_ptr<config::Config> config, std::string host, uint16_t port);
        bool has_supervised_channels(const config::Config& config) const;
        
        std::atomic<bool> running_{false};
        std::atomic<bool> shutdown_requested_{false};
        std::vector<std::unique_ptr<ComponentSupervisor>> supervisors_;
        std::unique_ptr<StateWriter> state_writer_;
        std::thread thread_;
        std::promise<DaemonResult> promise_;
        std::mutex mutex_;
    };

    std::future<DaemonResult> run(std::shared_ptr<config::Config> config, std::string host, uint16_t port);
}
}
