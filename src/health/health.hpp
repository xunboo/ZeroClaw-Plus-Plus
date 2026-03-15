#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace zeroclaw::health {

struct ComponentHealth {
    std::string status;
    std::string updated_at;
    std::optional<std::string> last_ok;
    std::optional<std::string> last_error;
    std::uint64_t restart_count;
};

struct HealthSnapshot {
    std::uint32_t pid;
    std::string updated_at;
    std::uint64_t uptime_seconds;
    std::map<std::string, ComponentHealth> components;
};

void mark_component_ok(const std::string& component);
void mark_component_error(const std::string& component, const std::string& error);
void bump_component_restart(const std::string& component);
HealthSnapshot snapshot();

}


namespace zeroclaw::health {
nlohmann::json snapshot_json();
}
