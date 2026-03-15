#pragma once

#include "types.hpp"
#include <functional>
#include <string>
#include <vector>

namespace zeroclaw::cron {

constexpr size_t MAX_CRON_OUTPUT_BYTES = 16 * 1024;
constexpr const char* TRUNCATED_OUTPUT_MARKER = "\n...[truncated]";

CronJob add_job(const std::string& workspace_dir, int max_run_history, const std::string& expression, const std::string& command);

CronJob add_shell_job(
    const std::string& workspace_dir,
    int max_run_history,
    int max_tasks,
    const std::optional<std::string>& name,
    const Schedule& schedule,
    const std::string& command
);

CronJob add_agent_job(
    const std::string& workspace_dir,
    int max_run_history,
    int max_tasks,
    const std::optional<std::string>& name,
    const Schedule& schedule,
    const std::string& prompt,
    SessionTarget session_target,
    const std::optional<std::string>& model,
    const std::optional<DeliveryConfig>& delivery,
    bool delete_after_run
);

std::vector<CronJob> list_jobs(const std::string& workspace_dir);

CronJob get_job(const std::string& workspace_dir, const std::string& job_id);

void remove_job(const std::string& workspace_dir, const std::string& id);

std::vector<CronJob> due_jobs(const std::string& workspace_dir, int max_tasks, TimePoint now);

CronJob update_job(const std::string& workspace_dir, int max_tasks, const std::string& job_id, const CronJobPatch& patch);

void record_last_run(
    const std::string& workspace_dir,
    const std::string& job_id,
    TimePoint finished_at,
    bool success,
    const std::string& output
);

void reschedule_after_run(
    const std::string& workspace_dir,
    int max_tasks,
    const std::string& job_id,
    const Schedule& schedule,
    bool success,
    const std::string& output
);

void record_run(
    const std::string& workspace_dir,
    int max_run_history,
    const std::string& job_id,
    TimePoint started_at,
    TimePoint finished_at,
    const std::string& status,
    const std::optional<std::string>& output,
    int64_t duration_ms
);

std::vector<CronRun> list_runs(const std::string& workspace_dir, const std::string& job_id, size_t limit);

std::string truncate_cron_output(const std::string& output);

}
