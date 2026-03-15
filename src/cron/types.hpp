#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace zeroclaw::cron {

using Clock = std::chrono::system_clock;
using TimePoint = std::chrono::system_clock::time_point;

enum class JobType {
    Shell,
    Agent
};

std::string job_type_to_string(JobType type);
JobType job_type_from_string(const std::string& str);

enum class SessionTarget {
    Isolated,
    Main
};

std::string session_target_to_string(SessionTarget target);
SessionTarget session_target_from_string(const std::string& str);

struct ScheduleCron {
    std::string expr;
    std::optional<std::string> tz;
};

struct ScheduleAt {
    TimePoint at;
};

struct ScheduleEvery {
    std::chrono::milliseconds every_ms;
};

using Schedule = std::variant<ScheduleCron, ScheduleAt, ScheduleEvery>;

struct DeliveryConfig {
    std::string mode = "none";
    std::optional<std::string> channel;
    std::optional<std::string> to;
    bool best_effort = true;
};

struct CronJob {
    std::string id;
    std::string expression;
    Schedule schedule;
    std::string command;
    std::optional<std::string> prompt;
    std::optional<std::string> name;
    JobType job_type = JobType::Shell;
    SessionTarget session_target = SessionTarget::Isolated;
    std::optional<std::string> model;
    bool enabled = true;
    DeliveryConfig delivery;
    bool delete_after_run = false;
    TimePoint created_at;
    TimePoint next_run;
    std::optional<TimePoint> last_run;
    std::optional<std::string> last_status;
    std::optional<std::string> last_output;
};

struct CronRun {
    int64_t id;
    std::string job_id;
    TimePoint started_at;
    TimePoint finished_at;
    std::string status;
    std::optional<std::string> output;
    std::optional<int64_t> duration_ms;
};

struct CronJobPatch {
    std::optional<Schedule> schedule;
    std::optional<std::string> command;
    std::optional<std::string> prompt;
    std::optional<std::string> name;
    std::optional<bool> enabled;
    std::optional<DeliveryConfig> delivery;
    std::optional<std::string> model;
    std::optional<SessionTarget> session_target;
    std::optional<bool> delete_after_run;
};

}
