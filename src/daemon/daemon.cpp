#include "daemon.hpp"
#include "../config/config.hpp"
#include "../health/health.hpp"

#include <iostream>
#include "../gateway/gateway.hpp"
#include "../channels_module.hpp"
#include "../observability/observer.hpp"
#include "../heartbeat/heartbeat.hpp"
#include "../agent/agent_module.hpp"
#include "../cron/cron.hpp"
#include <fstream>
#include <algorithm>

namespace zeroclaw {
namespace daemon {

std::filesystem::path state_file_path(const config::Config& config) {
    auto parent = config.config_path.parent_path();
    if (parent.empty()) {
        parent = std::filesystem::path(".");
    }
    return parent / "daemon_state.json";
}

ComponentSupervisor::ComponentSupervisor(const std::string& name,
                                         uint64_t initial_backoff_secs,
                                         uint64_t max_backoff_secs,
                                         ComponentFunc run_component)
    : name_(name)
    , initial_backoff_secs_(std::max(initial_backoff_secs, 1ULL))
    , max_backoff_secs_(std::max(max_backoff_secs, initial_backoff_secs_))
    , run_component_(std::move(run_component))
{
}

ComponentSupervisor::~ComponentSupervisor() {
    stop();
}

void ComponentSupervisor::start() {
    if (running_.exchange(true)) {
        return;
    }
    aborted_ = false;
    thread_ = std::thread(&ComponentSupervisor::supervisor_loop, this);
}

void ComponentSupervisor::stop() {
    abort();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ComponentSupervisor::abort() {
    aborted_ = true;
    cv_.notify_all();
}

void ComponentSupervisor::supervisor_loop() {
    uint64_t backoff = initial_backoff_secs_;
    uint64_t max_backoff = max_backoff_secs_;

    while (running_ && !aborted_) {
        health::mark_component_ok(name_);
        
        auto future = run_component_();
        auto result = future.get();
        
        if (result.success) {
            health::mark_component_error(name_, "component exited unexpectedly");
            backoff = initial_backoff_secs_;
        } else {
            health::mark_component_error(name_, result.error);
        }
        
        health::bump_component_restart(name_);
        
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(backoff), [this] { return !running_ || aborted_; });
        
        backoff = std::min(backoff * 2, max_backoff);
    }
}

StateWriter::StateWriter(std::shared_ptr<config::Config> config)
    : config_(std::move(config))
    , path_(state_file_path(*config_))
{
}

StateWriter::~StateWriter() {
    stop();
}

void StateWriter::start() {
    if (running_.exchange(true)) {
        return;
    }
    
    if (path_.has_parent_path()) {
        std::filesystem::create_directories(path_.parent_path());
    }
    
    thread_ = std::thread(&StateWriter::writer_loop, this);
}

void StateWriter::stop() {
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void StateWriter::writer_loop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(STATUS_FLUSH_SECONDS), [this] { return !running_; });
        
        if (!running_) break;
        
        std::string json_str = health::snapshot_json().dump();
        
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &now_time_t);
#else
        localtime_r(&now_time_t, &tm_buf);
#endif
        char time_buf[64];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
        
        std::string written_at = "\"written_at\":\"" + std::string(time_buf) + "\"";
        
        if (json_str.back() == '}') {
            json_str.pop_back();
            if (json_str.size() > 1 && json_str.back() != '{') {
                json_str += ",";
            }
            json_str += written_at + "}";
        }
        
        std::ofstream file(path_, std::ios::binary);
        if (file) {
            file << json_str;
        }
    }
}

DaemonRunner::DaemonRunner() = default;

DaemonRunner::~DaemonRunner() {
    shutdown();
}

std::future<DaemonResult> DaemonRunner::run(std::shared_ptr<config::Config> config, std::string host, uint16_t port) {
    running_ = true;
    shutdown_requested_ = false;
    
    auto future = promise_.get_future();
    
    thread_ = std::thread(&DaemonRunner::run_internal, this, std::move(config), std::move(host), port);
    
    return future;
}

void DaemonRunner::shutdown() {
    shutdown_requested_ = true;
    running_ = false;
    
    for (auto& supervisor : supervisors_) {
        supervisor->abort();
    }
    
    if (state_writer_) {
        state_writer_->stop();
    }
    
    if (thread_.joinable()) {
        thread_.join();
    }
    
    for (auto& supervisor : supervisors_) {
        supervisor->stop();
    }
    supervisors_.clear();
}

void DaemonRunner::run_internal(std::shared_ptr<config::Config> config, std::string host, uint16_t port) {
    try {
        uint64_t initial_backoff = std::max(config->reliability.channel_initial_backoff_secs, 1ULL);
        uint64_t max_backoff = std::max(config->reliability.channel_max_backoff_secs, initial_backoff);

        health::mark_component_ok("daemon");

        state_writer_ = std::make_unique<StateWriter>(config);
        state_writer_->start();

        {
            auto gateway_cfg = config;
            auto gateway_host = host;
            supervisors_.push_back(std::make_unique<ComponentSupervisor>(
                "gateway",
                initial_backoff,
                max_backoff,
                [gateway_cfg, gateway_host, port]() -> std::future<DaemonResult> {
                    return std::async(std::launch::async, [gateway_cfg, gateway_host, port]() {
                        DaemonResult res;
                        try {
                            auto err = zeroclaw::gateway::run_gateway(gateway_host, port, *gateway_cfg);
                            if (err) {
                                res.success = false;
                                res.error = *err;
                            } else {
                                res.success = true;
                            }
                        } catch (const std::exception& e) {
                            res.success = false;
                            res.error = e.what();
                        }
                        return res;
                    });
                }
            ));
            supervisors_.back()->start();
        }

        if (has_supervised_channels(*config)) {
            auto channels_cfg = config;
            supervisors_.push_back(std::make_unique<ComponentSupervisor>(
                "channels",
                initial_backoff,
                max_backoff,
                [channels_cfg]() -> std::future<DaemonResult> {
                    return std::async(std::launch::async, [channels_cfg]() {
                        DaemonResult res;
                        try {
                            channels::start_channels(*channels_cfg);
                            res.success = true;
                        } catch (const std::exception& e) {
                            res.success = false;
                            res.error = e.what();
                        }
                        return res;
                    });
                }
            ));
            supervisors_.back()->start();
        } else {
            health::mark_component_ok("channels");
        }

        if (config->heartbeat.enabled) {
            auto heartbeat_cfg = config;
            supervisors_.push_back(std::make_unique<ComponentSupervisor>(
                "heartbeat",
                initial_backoff,
                max_backoff,
                [heartbeat_cfg]() -> std::future<DaemonResult> {
                    return std::async(std::launch::async, [heartbeat_cfg]() {
                        DaemonResult res;
                        try {
                            auto observer = observability::create_observer(heartbeat_cfg->observability);
                            heartbeat::HeartbeatEngine engine(
                                heartbeat_cfg->heartbeat,
                                heartbeat_cfg->workspace_dir,
                                observer
                            );
                            
                            uint64_t interval_secs = std::max(heartbeat_cfg->heartbeat.interval_minutes, 5U) * 60;
                            
                            while (true) {
                                std::this_thread::sleep_for(std::chrono::seconds(interval_secs));
                                
                                auto tasks = engine.collect_tasks();
                                for (const auto& task : tasks) {
                                    std::string prompt = "[Heartbeat Task] " + task;
                                    try {
                                        agent::run(*heartbeat_cfg, prompt, std::nullopt, std::nullopt,
                                                   heartbeat_cfg->default_temperature, {}, false);
                                        health::mark_component_ok("heartbeat");
                                    } catch (const std::exception& e) {
                                        health::mark_component_error("heartbeat", e.what());
                                    }
                                }
                            }
                            res.success = true;
                        } catch (const std::exception& e) {
                            res.success = false;
                            res.error = e.what();
                        }
                        return res;
                    });
                }
            ));
            supervisors_.back()->start();
        }

        if (config->cron.enabled) {
            auto scheduler_cfg = config;
            supervisors_.push_back(std::make_unique<ComponentSupervisor>(
                "scheduler",
                initial_backoff,
                max_backoff,
                [scheduler_cfg]() -> std::future<DaemonResult> {
                    return std::async(std::launch::async, [scheduler_cfg]() {
                        DaemonResult res;
                        try {
                            cron::scheduler::run(*scheduler_cfg);
                            res.success = true;
                        } catch (const std::exception& e) {
                            res.success = false;
                            res.error = e.what();
                        }
                        return res;
                    });
                }
            ));
            supervisors_.back()->start();
        } else {
            health::mark_component_ok("scheduler");
        }

        std::cout << "ZeroClaw daemon started\n";
        std::cout << "   Gateway:  http://" << host << ":" << port << "\n";
        std::cout << "   Components: gateway, channels, heartbeat, scheduler\n";
        std::cout << "   Ctrl+C to stop\n";

        while (running_ && !shutdown_requested_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        health::mark_component_error("daemon", "shutdown requested");
        
        for (auto& supervisor : supervisors_) {
            supervisor->abort();
        }
        
        for (auto& supervisor : supervisors_) {
            supervisor->stop();
        }
        
        if (state_writer_) {
            state_writer_->stop();
        }

        promise_.set_value({true, ""});
    } catch (const std::exception& e) {
        promise_.set_value({false, e.what()});
    }
}

bool DaemonRunner::has_supervised_channels(const config::Config& config) const {
    const auto& cc = config.channels_config;
    return cc.telegram.has_value() ||
           cc.discord.has_value() ||
           cc.slack.has_value() ||
           cc.dingtalk.has_value() ||
           cc.mattermost.has_value() ||
           cc.qq.has_value() ||
           cc.nextcloud_talk.has_value();
}

std::future<DaemonResult> run(std::shared_ptr<config::Config> config, std::string host, uint16_t port) {
    static std::unique_ptr<DaemonRunner> runner;
    runner = std::make_unique<DaemonRunner>();
    return runner->run(std::move(config), std::move(host), port);
}

}
}
