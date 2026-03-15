#pragma once

#include "types.hpp"
#include "schedule.hpp"
#include "store.hpp"
#include "scheduler.hpp"
#include <string>
#include "config/config.hpp"

namespace zeroclaw::cron {

std::string format_time_rfc3339(TimePoint tp);

TimePoint parse_time_rfc3339(const std::string& str);

std::chrono::milliseconds parse_delay(const std::string& input);

/// Validate a shell command against security policy (allowed commands check).
/// Throws on failure.
void validate_shell_command(const config::Config& config, const std::string& command, bool approved);

/// Create a validated shell job, enforcing security policy before persistence.
CronJob add_shell_job_with_approval(
    const config::Config& config,
    const std::optional<std::string>& name,
    const Schedule& schedule,
    const std::string& command,
    bool approved
);

/// Update a shell job's command with security validation.
CronJob update_shell_job_with_approval(
    const config::Config& config,
    const std::string& job_id,
    const CronJobPatch& patch,
    bool approved
);

/// Create a one-shot validated shell job from a delay string (e.g. "30m").
CronJob add_once_validated(
    const config::Config& config,
    const std::string& delay,
    const std::string& command,
    bool approved
);

/// Create a one-shot validated shell job at an absolute timestamp.
CronJob add_once_at_validated(
    const config::Config& config,
    TimePoint at,
    const std::string& command,
    bool approved
);

// Convenience wrappers (default approved=false)
CronJob add_once(
    const config::Config& config,
    const std::string& delay,
    const std::string& command
);

CronJob add_once_at(
    const config::Config& config,
    TimePoint at,
    const std::string& command
);

CronJob pause_job(const config::Config& config, const std::string& id);

CronJob resume_job(const config::Config& config, const std::string& id);

/// Handle a cron CLI subcommand. Returns 0 on success, throws on error.
void handle_command(const config::Config& config, const std::string& subcommand,
                    // for "add" subcommand
                    const std::string& expression = "",
                    const std::optional<std::string>& tz = std::nullopt,
                    const std::string& command_arg = "",
                    // for "add-at"
                    const std::string& at_str = "",
                    // for "add-every"
                    uint64_t every_ms = 0,
                    // for "once"
                    const std::string& delay_str = "",
                    // for "update"
                    const std::string& id_arg = "",
                    const std::optional<std::string>& new_expression = std::nullopt,
                    const std::optional<std::string>& new_tz = std::nullopt,
                    const std::optional<std::string>& new_command = std::nullopt,
                    const std::optional<std::string>& new_name = std::nullopt);

namespace scheduler {
    /// Start the background cron scheduler
    void run(const zeroclaw::config::Config& config);
}

}
