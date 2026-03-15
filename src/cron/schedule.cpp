#include "schedule.hpp"
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <vector>

namespace zeroclaw::cron {

namespace {

std::vector<std::string> split_whitespace(const std::string& s) {
    std::vector<std::string> result;
    std::istringstream iss(s);
    std::string word;
    while (iss >> word) {
        result.push_back(word);
    }
    return result;
}

}

std::string normalize_expression(const std::string& expression) {
    std::string trimmed = expression;
    size_t start = trimmed.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        throw std::invalid_argument("Empty cron expression");
    }
    size_t end = trimmed.find_last_not_of(" \t\n\r");
    trimmed = trimmed.substr(start, end - start + 1);
    
    auto fields = split_whitespace(trimmed);
    size_t field_count = fields.size();
    
    switch (field_count) {
        case 5: {
            std::string result = "0 ";
            for (size_t i = 0; i < fields.size(); ++i) {
                if (i > 0) result += " ";
                result += fields[i];
            }
            return result;
        }
        case 6:
        case 7:
            return trimmed;
        default:
            throw std::invalid_argument(
                "Invalid cron expression: " + expression + 
                " (expected 5, 6, or 7 fields, got " + std::to_string(field_count) + ")"
            );
    }
}

TimePoint next_run_for_schedule(const Schedule& schedule, TimePoint from) {
    if (auto* cron = std::get_if<ScheduleCron>(&schedule)) {
        throw std::runtime_error("Cron expression parsing not implemented: " + cron->expr);
    }
    
    if (auto* at = std::get_if<ScheduleAt>(&schedule)) {
        return at->at;
    }
    
    if (auto* every = std::get_if<ScheduleEvery>(&schedule)) {
        if (every->every_ms.count() == 0) {
            throw std::invalid_argument("Invalid schedule: every_ms must be > 0");
        }
        return from + every->every_ms;
    }
    
    throw std::invalid_argument("Unknown schedule type");
}

void validate_schedule(const Schedule& schedule, TimePoint now) {
    if (std::get_if<ScheduleCron>(&schedule)) {
        next_run_for_schedule(schedule, now);
        return;
    }
    
    if (auto* at = std::get_if<ScheduleAt>(&schedule)) {
        if (at->at <= now) {
            throw std::invalid_argument("Invalid schedule: 'at' must be in the future");
        }
        return;
    }
    
    if (auto* every = std::get_if<ScheduleEvery>(&schedule)) {
        if (every->every_ms.count() == 0) {
            throw std::invalid_argument("Invalid schedule: every_ms must be > 0");
        }
        return;
    }
}

std::optional<std::string> schedule_cron_expression(const Schedule& schedule) {
    if (auto* cron = std::get_if<ScheduleCron>(&schedule)) {
        return cron->expr;
    }
    return std::nullopt;
}

}
