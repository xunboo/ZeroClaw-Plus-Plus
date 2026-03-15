#include "cron.hpp"
#include "store.hpp"
#include "schedule.hpp"
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace zeroclaw::cron {

std::string format_time_rfc3339(TimePoint tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
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
#ifdef _WIN32
    auto tp = std::chrono::system_clock::from_time_t(_mkgmtime(&tm));
#else
    auto tp = std::chrono::system_clock::from_time_t(timegm(&tm));
#endif
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

// ── Security validation ─────────────────────────────────────────

void validate_shell_command(const config::Config& config, const std::string& command, bool /*approved*/) {
    // If allowed_commands is configured, check if the command's first word is allowed
    if (!config.autonomy.allowed_commands.empty()) {
        std::string first_word;
        size_t space = command.find(' ');
        if (space != std::string::npos) {
            first_word = command.substr(0, space);
        } else {
            first_word = command;
        }

        bool found = false;
        for (const auto& allowed : config.autonomy.allowed_commands) {
            if (first_word == allowed) {
                found = true;
                break;
            }
        }

        if (!found) {
            throw std::runtime_error("blocked by security policy: command not in allowlist: " + first_word);
        }
    }

    // Check forbidden paths
    for (const auto& path : config.autonomy.forbidden_paths) {
        if (command.find(path) != std::string::npos) {
            throw std::runtime_error("blocked by security policy: forbidden path argument: " + path);
        }
    }
}

CronJob add_shell_job_with_approval(
    const config::Config& config,
    const std::optional<std::string>& name,
    const Schedule& schedule,
    const std::string& command,
    bool approved
) {
    validate_shell_command(config, command, approved);
    return add_shell_job(
        config.workspace_dir.string(),
        static_cast<int>(config.cron.max_run_history),
        static_cast<int>(config.scheduler.max_tasks),
        name, schedule, command
    );
}

CronJob update_shell_job_with_approval(
    const config::Config& config,
    const std::string& job_id,
    const CronJobPatch& patch,
    bool approved
) {
    if (patch.command) {
        validate_shell_command(config, *patch.command, approved);
    }
    return update_job(
        config.workspace_dir.string(),
        static_cast<int>(config.scheduler.max_tasks),
        job_id, patch
    );
}

CronJob add_once_validated(
    const config::Config& config,
    const std::string& delay,
    const std::string& command,
    bool approved
) {
    auto duration = parse_delay(delay);
    auto at = std::chrono::system_clock::now() + duration;
    return add_once_at_validated(config, at, command, approved);
}

CronJob add_once_at_validated(
    const config::Config& config,
    TimePoint at,
    const std::string& command,
    bool approved
) {
    Schedule schedule = ScheduleAt{at};
    return add_shell_job_with_approval(config, std::nullopt, schedule, command, approved);
}

CronJob add_once(
    const config::Config& config,
    const std::string& delay,
    const std::string& command
) {
    return add_once_validated(config, delay, command, false);
}

CronJob add_once_at(
    const config::Config& config,
    TimePoint at,
    const std::string& command
) {
    return add_once_at_validated(config, at, command, false);
}

CronJob pause_job(const config::Config& config, const std::string& id) {
    CronJobPatch patch;
    patch.enabled = false;
    return update_job(config.workspace_dir.string(),
                      static_cast<int>(config.scheduler.max_tasks), id, patch);
}

CronJob resume_job(const config::Config& config, const std::string& id) {
    CronJobPatch patch;
    patch.enabled = true;
    return update_job(config.workspace_dir.string(),
                      static_cast<int>(config.scheduler.max_tasks), id, patch);
}

// ── CLI command handler (matching Rust cron::handle_command) ─────

void handle_command(const config::Config& config, const std::string& subcommand,
                    const std::string& expression,
                    const std::optional<std::string>& tz,
                    const std::string& command_arg,
                    const std::string& at_str,
                    uint64_t every_ms,
                    const std::string& delay_str,
                    const std::string& id_arg,
                    const std::optional<std::string>& new_expression,
                    const std::optional<std::string>& new_tz,
                    const std::optional<std::string>& new_command,
                    const std::optional<std::string>& new_name) {

    std::string ws = config.workspace_dir.string();

    if (subcommand == "list") {
        auto jobs = list_jobs(ws);
        if (jobs.empty()) {
            std::cout << "No scheduled tasks yet.\n";
            std::cout << "\nUsage:\n";
            std::cout << "  zeroclaw++ cron add '0 9 * * *' 'agent -m \"Good morning!\"'\n";
            return;
        }

        std::cout << "Scheduled jobs (" << jobs.size() << "):\n";
        for (const auto& job : jobs) {
            std::string last_run = job.last_run
                ? format_time_rfc3339(*job.last_run)
                : "never";
            std::string last_status = job.last_status.value_or("n/a");
            std::cout << "- " << job.id
                      << " | " << job.expression
                      << " | next=" << format_time_rfc3339(job.next_run)
                      << " | last=" << last_run << " (" << last_status << ")"
                      << (job.enabled ? "" : " [PAUSED]")
                      << "\n";
            if (!job.command.empty()) {
                std::cout << "    cmd: " << job.command << "\n";
            }
            if (job.prompt) {
                std::cout << "    prompt: " << *job.prompt << "\n";
            }
        }
    } else if (subcommand == "add") {
        Schedule schedule = ScheduleCron{expression, tz};
        auto job = add_shell_job_with_approval(config, std::nullopt, schedule, command_arg, false);
        std::cout << "Added cron job " << job.id << "\n";
        std::cout << "  Expr: " << job.expression << "\n";
        std::cout << "  Next: " << format_time_rfc3339(job.next_run) << "\n";
        std::cout << "  Cmd : " << job.command << "\n";
    } else if (subcommand == "add-at") {
        auto at = parse_time_rfc3339(at_str);
        Schedule schedule = ScheduleAt{at};
        auto job = add_shell_job_with_approval(config, std::nullopt, schedule, command_arg, false);
        std::cout << "Added one-shot cron job " << job.id << "\n";
        std::cout << "  At  : " << format_time_rfc3339(job.next_run) << "\n";
        std::cout << "  Cmd : " << job.command << "\n";
    } else if (subcommand == "add-every") {
        Schedule schedule = ScheduleEvery{std::chrono::milliseconds(every_ms)};
        auto job = add_shell_job_with_approval(config, std::nullopt, schedule, command_arg, false);
        std::cout << "Added interval cron job " << job.id << "\n";
        std::cout << "  Every(ms): " << every_ms << "\n";
        std::cout << "  Next     : " << format_time_rfc3339(job.next_run) << "\n";
        std::cout << "  Cmd      : " << job.command << "\n";
    } else if (subcommand == "once") {
        auto job = add_once(config, delay_str, command_arg);
        std::cout << "Added one-shot cron job " << job.id << "\n";
        std::cout << "  At  : " << format_time_rfc3339(job.next_run) << "\n";
        std::cout << "  Cmd : " << job.command << "\n";
    } else if (subcommand == "remove") {
        remove_job(ws, id_arg);
        std::cout << "Removed cron job " << id_arg << "\n";
    } else if (subcommand == "pause") {
        pause_job(config, id_arg);
        std::cout << "Paused cron job " << id_arg << "\n";
    } else if (subcommand == "resume") {
        resume_job(config, id_arg);
        std::cout << "Resumed cron job " << id_arg << "\n";
    } else if (subcommand == "update") {
        if (!new_expression && !new_tz && !new_command && !new_name) {
            throw std::runtime_error("At least one of --expression, --tz, --command, or --name must be provided");
        }

        // Merge expression/tz with existing schedule
        std::optional<Schedule> schedule;
        if (new_expression || new_tz) {
            auto existing = get_job(ws, id_arg);
            auto* cron_sched = std::get_if<ScheduleCron>(&existing.schedule);
            if (!cron_sched) {
                throw std::runtime_error("Cannot update expression/tz on a non-cron schedule");
            }
            schedule = ScheduleCron{
                new_expression.value_or(cron_sched->expr),
                new_tz ? new_tz : cron_sched->tz
            };
        }

        CronJobPatch patch;
        patch.schedule = schedule;
        patch.command = new_command;
        patch.name = new_name;

        auto job = update_shell_job_with_approval(config, id_arg, patch, false);
        std::cout << "Updated cron job " << job.id << "\n";
        std::cout << "  Expr: " << job.expression << "\n";
        std::cout << "  Next: " << format_time_rfc3339(job.next_run) << "\n";
        std::cout << "  Cmd : " << job.command << "\n";
    } else {
        throw std::runtime_error("Unknown cron subcommand: " + subcommand);
    }
}

}
