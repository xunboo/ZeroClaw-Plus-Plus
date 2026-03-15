#pragma once

#include "types.hpp"
#include <string>

namespace zeroclaw::cron {

std::string normalize_expression(const std::string& expression);

TimePoint next_run_for_schedule(const Schedule& schedule, TimePoint from);

void validate_schedule(const Schedule& schedule, TimePoint now);

std::optional<std::string> schedule_cron_expression(const Schedule& schedule);

inline ScheduleCron* get_schedule_cron(Schedule& s) {
    return std::get_if<ScheduleCron>(&s);
}

inline const ScheduleCron* get_schedule_cron(const Schedule& s) {
    return std::get_if<ScheduleCron>(&s);
}

inline ScheduleAt* get_schedule_at(Schedule& s) {
    return std::get_if<ScheduleAt>(&s);
}

inline const ScheduleAt* get_schedule_at(const Schedule& s) {
    return std::get_if<ScheduleAt>(&s);
}

inline ScheduleEvery* get_schedule_every(Schedule& s) {
    return std::get_if<ScheduleEvery>(&s);
}

inline const ScheduleEvery* get_schedule_every(const Schedule& s) {
    return std::get_if<ScheduleEvery>(&s);
}

}
