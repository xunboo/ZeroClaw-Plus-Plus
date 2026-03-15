#include "store.hpp"
#include "schedule.hpp"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sqlite3.h>
#endif

namespace zeroclaw::cron {

namespace {

std::string escape_sql_string(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        if (c == '\'') result += "''";
        else result += c;
    }
    return result;
}

std::string format_rfc3339(TimePoint tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

TimePoint parse_rfc3339(const std::string& str) {
    std::tm tm = {};
    std::istringstream iss(str);
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    size_t dot_pos = str.find('.');
    if (dot_pos != std::string::npos) {
        std::string ms_str = str.substr(dot_pos + 1);
        ms_str = ms_str.substr(0, ms_str.find('Z'));
        while (ms_str.size() < 3) ms_str += '0';
        if (ms_str.size() > 3) ms_str = ms_str.substr(0, 3);
        int ms = std::stoi(ms_str);
        tp += std::chrono::milliseconds(ms);
    }
    return tp;
}

std::string schedule_to_json(const Schedule& schedule) {
    if (auto* cron = std::get_if<ScheduleCron>(&schedule)) {
        std::string json = R"({"kind":"cron","expr":")" + escape_sql_string(cron->expr) + "\"";
        if (cron->tz) {
            json += R"(,"tz":")" + escape_sql_string(*cron->tz) + "\"";
        }
        json += "}";
        return json;
    }
    if (auto* at = std::get_if<ScheduleAt>(&schedule)) {
        return R"({"kind":"at","at":")" + format_rfc3339(at->at) + "\"}";
    }
    if (auto* every = std::get_if<ScheduleEvery>(&schedule)) {
        return R"({"kind":"every","every_ms":)" + std::to_string(every->every_ms.count()) + "}";
    }
    return "{}";
}

std::string delivery_to_json(const DeliveryConfig& delivery) {
    std::string json = R"({"mode":")" + escape_sql_string(delivery.mode) + "\"";
    if (delivery.channel) {
        json += R"(,"channel":")" + escape_sql_string(*delivery.channel) + "\"";
    }
    if (delivery.to) {
        json += R"(,"to":")" + escape_sql_string(*delivery.to) + "\"";
    }
    json += R"(,"best_effort":)" + std::string(delivery.best_effort ? "true" : "false") + "}";
    return json;
}

}

std::string truncate_cron_output(const std::string& output) {
    if (output.size() <= MAX_CRON_OUTPUT_BYTES) {
        return output;
    }
    
    if (MAX_CRON_OUTPUT_BYTES <= std::strlen(TRUNCATED_OUTPUT_MARKER)) {
        return TRUNCATED_OUTPUT_MARKER;
    }
    
    size_t cutoff = MAX_CRON_OUTPUT_BYTES - std::strlen(TRUNCATED_OUTPUT_MARKER);
    std::string truncated = output.substr(0, cutoff);
    truncated += TRUNCATED_OUTPUT_MARKER;
    return truncated;
}

CronJob add_job(const std::string& workspace_dir, int max_run_history, const std::string& expression, const std::string& command) {
    Schedule schedule = ScheduleCron{expression, std::nullopt};
    return add_shell_job(workspace_dir, max_run_history, 100, std::nullopt, schedule, command);
}

CronJob add_shell_job(
    const std::string& workspace_dir,
    int max_run_history,
    int max_tasks,
    const std::optional<std::string>& name,
    const Schedule& schedule,
    const std::string& command
) {
    CronJob job;
    job.id = "stub_id";
    job.schedule = schedule;
    job.command = command;
    job.name = name;
    job.job_type = JobType::Shell;
    return job;
}

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
) {
    CronJob job;
    job.id = "stub_agent_job";
    job.schedule = schedule;
    job.prompt = prompt;
    job.name = name;
    job.job_type = JobType::Agent;
    job.session_target = session_target;
    job.model = model;
    if (delivery) job.delivery = *delivery;
    job.delete_after_run = delete_after_run;
    return job;
}

std::vector<CronJob> list_jobs(const std::string& workspace_dir) {
    return {};
}

CronJob get_job(const std::string& workspace_dir, const std::string& job_id) {
    return CronJob();
}

void remove_job(const std::string& workspace_dir, const std::string& id) {
    // Stub
}

std::vector<CronJob> due_jobs(const std::string& workspace_dir, int max_tasks, TimePoint now) {
    return {};
}

CronJob update_job(const std::string& workspace_dir, int max_tasks, const std::string& job_id, const CronJobPatch& patch) {
    return CronJob();
}

void record_last_run(
    const std::string& workspace_dir,
    const std::string& job_id,
    TimePoint finished_at,
    bool success,
    const std::string& output
) {
    // Stub
}

void reschedule_after_run(
    const std::string& workspace_dir,
    int max_tasks,
    const std::string& job_id,
    const Schedule& schedule,
    bool success,
    const std::string& output
) {
    // Stub
}

void record_run(
    const std::string& workspace_dir,
    int max_run_history,
    const std::string& job_id,
    TimePoint started_at,
    TimePoint finished_at,
    const std::string& status,
    const std::optional<std::string>& output,
    int64_t duration_ms
) {
    // Stub
}

std::vector<CronRun> list_runs(const std::string& workspace_dir, const std::string& job_id, size_t limit) {
    return {};
}

}

