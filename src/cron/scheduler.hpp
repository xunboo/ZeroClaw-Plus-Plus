#pragma once

#include "types.hpp"
#include <functional>
#include <memory>
#include <string>

namespace zeroclaw::cron {

struct SecurityPolicy;
struct Config;
struct ChannelsConfig;

struct JobResult {
    bool success;
    std::string output;
};

constexpr uint64_t MIN_POLL_SECONDS = 5;
constexpr uint64_t SHELL_JOB_TIMEOUT_SECS = 120;

void run_scheduler(
    const std::string& workspace_dir,
    int scheduler_poll_secs,
    int max_concurrent,
    int max_tasks,
    int scheduler_retries,
    int provider_backoff_ms,
    int max_run_history,
    const std::function<void(const std::string&, bool)>& health_marker,
    const std::function<bool()>& shutdown_requested
);

JobResult execute_job_now(
    const std::string& workspace_dir,
    int max_run_history,
    int max_tasks,
    int scheduler_retries,
    int provider_backoff_ms,
    const CronJob& job,
    const std::string& autonomy_level,
    const std::vector<std::string>& allowed_commands,
    const std::vector<std::string>& forbidden_paths
);

JobResult execute_shell_job(
    const std::string& workspace_dir,
    const CronJob& job,
    const std::string& autonomy_level,
    const std::vector<std::string>& allowed_commands,
    const std::vector<std::string>& forbidden_paths,
    uint64_t timeout_secs
);

bool is_one_shot_auto_delete(const CronJob& job);

void warn_if_high_frequency_agent_job(const CronJob& job);

}
