#include "cron.hpp"
#include "store.hpp"
#include "schedule.hpp"
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace zeroclaw::cron {

std::string format_time_rfc3339(TimePoint tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

TimePoint parse_time_rfc3339(const std::string& str) {
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

std::chrono::milliseconds parse_delay(const std::string& input) {
    std::string trimmed = input;
    size_t start = trimmed.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        throw std::invalid_argument("delay must not be empty");
    }
    size_t end = trimmed.find_last_not_of(" \t\n\r");
    trimmed = trimmed.substr(start, end - start + 1);
    
    size_t split = 0;
    for (size_t i = 0; i < trimmed.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(trimmed[i]))) {
            split = i;
            break;
        }
    }
    if (split == 0 && !trimmed.empty() && std::isdigit(static_cast<unsigned char>(trimmed[0]))) {
        split = trimmed.size();
    }
    
    std::string num_str = trimmed.substr(0, split);
    std::string unit = trimmed.substr(split);
    
    if (num_str.empty()) {
        throw std::invalid_argument("delay must start with a number");
    }
    
    int64_t amount = std::stoll(num_str);
    
    if (unit.empty()) {
        unit = "m";
    }
    
    if (unit == "s") {
        return std::chrono::seconds(amount);
    } else if (unit == "m") {
        return std::chrono::minutes(amount);
    } else if (unit == "h") {
        return std::chrono::hours(amount);
    } else if (unit == "d") {
        return std::chrono::hours(amount * 24);
    } else {
        throw std::invalid_argument("unsupported delay unit '" + unit + "', use s/m/h/d");
    }
}

CronJob add_once(
    const std::string& workspace_dir,
    int max_run_history,
    int max_tasks,
    const std::string& delay,
    const std::string& command
) {
    auto duration = parse_delay(delay);
    auto now = std::chrono::system_clock::now();
    auto at = now + duration;
    return add_once_at(workspace_dir, max_run_history, max_tasks, at, command);
}

CronJob add_once_at(
    const std::string& workspace_dir,
    int max_run_history,
    int max_tasks,
    TimePoint at,
    const std::string& command
) {
    Schedule schedule = ScheduleAt{at};
    return add_shell_job(workspace_dir, max_run_history, max_tasks, std::nullopt, schedule, command);
}

CronJob pause_job(const std::string& workspace_dir, int max_tasks, const std::string& id) {
    CronJobPatch patch;
    patch.enabled = false;
    return update_job(workspace_dir, max_tasks, id, patch);
}

CronJob resume_job(const std::string& workspace_dir, int max_tasks, const std::string& id) {
    CronJobPatch patch;
    patch.enabled = true;
    return update_job(workspace_dir, max_tasks, id, patch);
}

}
