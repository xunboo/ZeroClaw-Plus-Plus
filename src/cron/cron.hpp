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

CronJob add_once(
    const std::string& workspace_dir,
    int max_run_history,
    int max_tasks,
    const std::string& delay,
    const std::string& command
);

CronJob add_once_at(
    const std::string& workspace_dir,
    int max_run_history,
    int max_tasks,
    TimePoint at,
    const std::string& command
);

CronJob pause_job(const std::string& workspace_dir, int max_tasks, const std::string& id);

CronJob resume_job(const std::string& workspace_dir, int max_tasks, const std::string& id);

namespace scheduler {
    /// Start the background cron scheduler
    void run(const zeroclaw::config::Config& config);
}

}
