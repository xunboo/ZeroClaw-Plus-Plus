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
    void run(const zeroclaw::config::Config& config) {
        // Stub implementation
    }
}

}
