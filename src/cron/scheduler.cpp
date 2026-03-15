#include "scheduler.hpp"
#include "store.hpp"
#include "schedule.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <thread>
#include "config/config.hpp"

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace zeroclaw::cron {

bool is_one_shot_auto_delete(const CronJob& job) {
    return job.delete_after_run && std::holds_alternative<ScheduleAt>(job.schedule);
}

void warn_if_high_frequency_agent_job(const CronJob& job) {
    if (job.job_type != JobType::Agent) {
        return;
    }
    
    bool too_frequent = false;
    
    if (auto* every = std::get_if<ScheduleEvery>(&job.schedule)) {
        too_frequent = every->every_ms.count() < 5 * 60 * 1000;
    } else if (std::get_if<ScheduleCron>(&job.schedule)) {
        auto now = std::chrono::system_clock::now();
        try {
            auto next1 = next_run_for_schedule(job.schedule, now);
            auto next2 = next_run_for_schedule(job.schedule, now + std::chrono::seconds(1));
            auto diff = std::chrono::duration_cast<std::chrono::minutes>(next2 - next1);
            too_frequent = diff.count() < 5;
        } catch (...) {
            too_frequent = false;
        }
    }
    
    if (too_frequent) {
        std::fprintf(stderr, "Warning: Cron agent job '%s' is scheduled more frequently than every 5 minutes\n", job.id.c_str());
    }
}

JobResult execute_shell_job(
    const std::string& workspace_dir,
    const CronJob& job,
    const std::string& autonomy_level,
    const std::vector<std::string>& allowed_commands,
    const std::vector<std::string>& forbidden_paths,
    uint64_t timeout_secs
) {
    if (autonomy_level == "readonly") {
        return {false, "blocked by security policy: autonomy is read-only"};
    }
    
    if (!allowed_commands.empty()) {
        std::string first_word;
        size_t space = job.command.find(' ');
        if (space != std::string::npos) {
            first_word = job.command.substr(0, space);
        } else {
            first_word = job.command;
        }
        
        bool found = false;
        for (const auto& cmd : allowed_commands) {
            if (first_word == cmd) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            return {false, "blocked by security policy: command not allowed: " + job.command};
        }
    }
    
    for (const auto& path : forbidden_paths) {
        if (job.command.find(path) != std::string::npos) {
            return {false, "blocked by security policy: forbidden path argument: " + path};
        }
    }
    
    std::string full_cmd = "cd " + workspace_dir + " && " + job.command + " 2>&1";
    
    FILE* pipe = nullptr;
    std::string output;
    char buffer[128];
    
    pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
        return {false, "spawn error: failed to execute command"};
    }
    
    auto start = std::chrono::steady_clock::now();
    bool timed_out = false;
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start);
        if (static_cast<uint64_t>(elapsed.count()) > timeout_secs) {
            timed_out = true;
            break;
        }
    }
    
    int status = pclose(pipe);
    
    if (timed_out) {
        return {false, "job timed out after " + std::to_string(timeout_secs) + "s"};
    }
    
    bool success = (status == 0);
    std::ostringstream oss;
    oss << "status=" << status << "\noutput:\n" << output;
    
    return {success, oss.str()};
}

namespace scheduler {

// Matching Rust: MIN_POLL_SECONDS = 5
static constexpr uint64_t MIN_POLL_SECONDS = 5;
// Matching Rust: SHELL_JOB_TIMEOUT_SECS = 120
static constexpr uint64_t SHELL_JOB_TIMEOUT_SECS = 120;

/// Execute a job with retry+backoff, matching Rust execute_job_with_retry()
static JobResult execute_with_retry(
    const zeroclaw::config::Config& config,
    const CronJob& job,
    int retries,
    uint64_t backoff_ms)
{
    JobResult last{false, ""};

    const std::string autonomy = config.autonomy.level;
    const auto& allowed = config.autonomy.allowed_commands;
    const auto& forbidden = config.autonomy.forbidden_paths;

    for (int attempt = 0; attempt <= retries; ++attempt) {
        last = execute_shell_job(
            config.workspace_dir.string(),
            job, autonomy, allowed, forbidden,
            SHELL_JOB_TIMEOUT_SECS);

        if (last.success) return last;

        // Matching Rust: deterministic policy violations are not retryable
        if (last.output.rfind("blocked by security policy:", 0) == 0) return last;

        if (attempt < retries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
            backoff_ms = std::min(backoff_ms * 2, uint64_t{30000});
        }
    }
    return last;
}

void run(const zeroclaw::config::Config& config) {
    // Matching Rust: poll_secs = max(config.reliability.scheduler_poll_secs, MIN_POLL_SECONDS)
    uint64_t poll_secs = std::max(
        static_cast<uint64_t>(config.reliability.scheduler_poll_secs),
        MIN_POLL_SECONDS);

    int retries = config.reliability.scheduler_retries;
    uint64_t backoff_ms = std::max(config.reliability.provider_backoff_ms, uint64_t{200});

    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(poll_secs));

        // Query due jobs (matching Rust: due_jobs(&config, Utc::now()))
        auto now = std::chrono::system_clock::now();
        std::vector<CronJob> due;
        try {
            due = due_jobs(config.workspace_dir.string(), static_cast<int>(config.scheduler.max_tasks), now);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[scheduler] query failed: %s\n", e.what());
            continue;
        }

        // Execute each due job
        for (const auto& job : due) {
            warn_if_high_frequency_agent_job(job);

            auto started_at = std::chrono::system_clock::now();
            auto result = execute_with_retry(config, job, retries, backoff_ms);
            auto finished_at = std::chrono::system_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                finished_at - started_at).count();

            if (!result.success) {
                std::fprintf(stderr, "[scheduler] job '%s' failed: %s\n",
                             job.id.c_str(), result.output.c_str());
            }

            // Persist run record (matching Rust: record_run)
            try {
                record_run(config.workspace_dir.string(), static_cast<int>(config.cron.max_run_history), job.id, started_at, finished_at,
                           result.success ? "ok" : "error",
                           result.output, duration_ms);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "[scheduler] record_run failed: %s\n", e.what());
            }

            // One-shot auto-delete (matching Rust: is_one_shot_auto_delete)
            if (is_one_shot_auto_delete(job)) {
                if (result.success) {
                    try { remove_job(config.workspace_dir.string(), job.id); } catch (...) {}
                } else {
                    try {
                        CronJobPatch patch;
                        patch.enabled = false;
                        update_job(config.workspace_dir.string(), static_cast<int>(config.scheduler.max_tasks), job.id, patch);
                    } catch (...) {}
                }
                continue;
            }

            // Reschedule after run (matching Rust: reschedule_after_run)
            try {
                reschedule_after_run(config.workspace_dir.string(), static_cast<int>(config.scheduler.max_tasks), job.id, job.schedule, result.success, result.output);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "[scheduler] reschedule failed: %s\n", e.what());
            }
        }
    }
}

} // namespace scheduler


}
