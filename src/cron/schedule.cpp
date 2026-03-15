#include "schedule.hpp"
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <vector>
#include <set>
#include <algorithm>
#include <ctime>

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

// Parse a single cron field into a set of valid values.
// Supports: * (all), N (single), N-M (range), N-M/S (range step), */S (wild step), N,M,... (list)
std::set<int> parse_cron_field(const std::string& field, int min_val, int max_val) {
    std::set<int> result;
    
    // Split by comma for list support
    std::vector<std::string> parts;
    std::istringstream iss(field);
    std::string part;
    while (std::getline(iss, part, ',')) {
        parts.push_back(part);
    }
    
    for (const auto& p : parts) {
        // Check for step: */S or N-M/S
        size_t slash_pos = p.find('/');
        int step = 1;
        std::string base = p;
        if (slash_pos != std::string::npos) {
            step = std::stoi(p.substr(slash_pos + 1));
            base = p.substr(0, slash_pos);
            if (step <= 0) step = 1;
        }
        
        // Check for range: N-M or wildcard *
        if (base == "*") {
            for (int i = min_val; i <= max_val; i += step) {
                result.insert(i);
            }
        } else {
            size_t dash_pos = base.find('-');
            if (dash_pos != std::string::npos) {
                int start = std::stoi(base.substr(0, dash_pos));
                int end = std::stoi(base.substr(dash_pos + 1));
                for (int i = start; i <= end; i += step) {
                    if (i >= min_val && i <= max_val) {
                        result.insert(i);
                    }
                }
            } else {
                // Single value
                int val = std::stoi(base);
                if (val >= min_val && val <= max_val) {
                    result.insert(val);
                }
            }
        }
    }
    
    return result;
}

// Compute next run from a 5-field cron expression (minute hour dom month dow).
// Returns the next time >= from that matches the expression.
TimePoint next_run_from_cron_fields(const std::string& expr, TimePoint from) {
    auto fields = split_whitespace(expr);
    
    // Normalize: if 6-7 fields, strip leading seconds; if 5 fields, use as-is
    std::string minute_f, hour_f, dom_f, month_f, dow_f;
    if (fields.size() == 5) {
        minute_f = fields[0];
        hour_f = fields[1];
        dom_f = fields[2];
        month_f = fields[3];
        dow_f = fields[4];
    } else if (fields.size() == 6) {
        // seconds minute hour dom month dow → skip seconds
        minute_f = fields[1];
        hour_f = fields[2];
        dom_f = fields[3];
        month_f = fields[4];
        dow_f = fields[5];
    } else if (fields.size() == 7) {
        // seconds minute hour dom month dow year → skip seconds and year
        minute_f = fields[1];
        hour_f = fields[2];
        dom_f = fields[3];
        month_f = fields[4];
        dow_f = fields[5];
    } else {
        throw std::invalid_argument("Invalid cron expression: " + expr);
    }
    
    auto minutes = parse_cron_field(minute_f, 0, 59);
    auto hours = parse_cron_field(hour_f, 0, 23);
    auto doms = parse_cron_field(dom_f, 1, 31);
    auto months = parse_cron_field(month_f, 1, 12);
    auto dows = parse_cron_field(dow_f, 0, 6);  // 0=Sunday
    
    // Start searching from 'from' + 1 minute, rounded to the minute boundary
    auto t = from + std::chrono::seconds(60);
    auto time = std::chrono::system_clock::to_time_t(t);
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    tm.tm_sec = 0; // Round to start of minute
    
    // Search up to 4 years ahead (to cover all edge cases)
    constexpr int MAX_ITERATIONS = 4 * 366 * 24 * 60; // ~4 years of minutes
    
    for (int iter = 0; iter < MAX_ITERATIONS; ++iter) {
        int month = tm.tm_mon + 1;  // tm_mon is 0-based
        int dom = tm.tm_mday;
        int dow = tm.tm_wday;       // 0=Sunday
        int hour = tm.tm_hour;
        int minute = tm.tm_min;
        
        // Check month
        if (months.find(month) == months.end()) {
            // Advance to next valid month
            tm.tm_mday = 1;
            tm.tm_hour = 0;
            tm.tm_min = 0;
            tm.tm_mon += 1;
            if (tm.tm_mon >= 12) {
                tm.tm_mon = 0;
                tm.tm_year += 1;
            }
#ifdef _WIN32
            auto norm_t = _mkgmtime(&tm);
            gmtime_s(&tm, &norm_t);
#else
            auto norm_t = timegm(&tm);
            gmtime_r(&norm_t, &tm);
#endif
            continue;
        }
        
        // Check day of month and day of week
        bool dom_match = doms.find(dom) != doms.end();
        bool dow_match = dows.find(dow) != dows.end();
        
        // Standard cron: if both DOM and DOW are restricted (not *), then either can match
        // If one is *, only the other needs to match
        bool dom_all = (dom_f == "*");
        bool dow_all = (dow_f == "*");
        
        bool day_match;
        if (dom_all && dow_all) {
            day_match = true;
        } else if (dom_all) {
            day_match = dow_match;
        } else if (dow_all) {
            day_match = dom_match;
        } else {
            // Both restricted: OR semantics (standard cron behavior)
            day_match = dom_match || dow_match;
        }
        
        if (!day_match) {
            // Advance to next day
            tm.tm_mday += 1;
            tm.tm_hour = 0;
            tm.tm_min = 0;
#ifdef _WIN32
            auto norm_t = _mkgmtime(&tm);
            gmtime_s(&tm, &norm_t);
#else
            auto norm_t = timegm(&tm);
            gmtime_r(&norm_t, &tm);
#endif
            continue;
        }
        
        // Check hour
        if (hours.find(hour) == hours.end()) {
            // Advance to next valid hour
            auto it = hours.upper_bound(hour);
            if (it != hours.end()) {
                tm.tm_hour = *it;
                tm.tm_min = 0;
            } else {
                // Next day
                tm.tm_mday += 1;
                tm.tm_hour = 0;
                tm.tm_min = 0;
            }
#ifdef _WIN32
            auto norm_t = _mkgmtime(&tm);
            gmtime_s(&tm, &norm_t);
#else
            auto norm_t = timegm(&tm);
            gmtime_r(&norm_t, &tm);
#endif
            continue;
        }
        
        // Check minute
        if (minutes.find(minute) == minutes.end()) {
            auto it = minutes.upper_bound(minute);
            if (it != minutes.end()) {
                tm.tm_min = *it;
            } else {
                // Next hour
                tm.tm_hour += 1;
                tm.tm_min = 0;
#ifdef _WIN32
                auto norm_t = _mkgmtime(&tm);
                gmtime_s(&tm, &norm_t);
#else
                auto norm_t = timegm(&tm);
                gmtime_r(&norm_t, &tm);
#endif
                continue;
            }
        }
        
        // All fields match! Build TimePoint
        tm.tm_sec = 0;
#ifdef _WIN32
        auto result_t = _mkgmtime(&tm);
#else
        auto result_t = timegm(&tm);
#endif
        return std::chrono::system_clock::from_time_t(result_t);
    }
    
    throw std::runtime_error("Could not find next run for cron expression: " + expr + " (searched 4 years)");
}

} // anonymous namespace

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
        return next_run_from_cron_fields(cron->expr, from);
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
