#include "health.hpp"
#include <chrono>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace zeroclaw::health {

namespace {

std::string now_rfc3339() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

std::uint32_t get_pid() {
#ifdef _WIN32
    return static_cast<std::uint32_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint32_t>(getpid());
#endif
}

struct HealthRegistry {
    std::chrono::steady_clock::time_point started_at;
    std::mutex mutex;
    std::map<std::string, ComponentHealth> components;

    HealthRegistry() : started_at(std::chrono::steady_clock::now()) {}
};

HealthRegistry& registry() {
    static HealthRegistry instance;
    return instance;
}

template<typename F>
void upsert_component(const std::string& component, F update) {
    auto& reg = registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    auto now = now_rfc3339();
    auto it = reg.components.find(component);
    if (it == reg.components.end()) {
        ComponentHealth entry{
            "starting",
            now,
            std::nullopt,
            std::nullopt,
            0
        };
        update(entry);
        entry.updated_at = now;
        reg.components[component] = entry;
    } else {
        update(it->second);
        it->second.updated_at = now;
    }
}

}

void mark_component_ok(const std::string& component) {
    upsert_component(component, [](ComponentHealth& entry) {
        entry.status = "ok";
        entry.last_ok = now_rfc3339();
        entry.last_error = std::nullopt;
    });
}

void mark_component_error(const std::string& component, const std::string& error) {
    upsert_component(component, [&error](ComponentHealth& entry) {
        entry.status = "error";
        entry.last_error = error;
    });
}

void bump_component_restart(const std::string& component) {
    upsert_component(component, [](ComponentHealth& entry) {
        entry.restart_count += 1;
    });
}

HealthSnapshot snapshot() {
    auto& reg = registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - reg.started_at);
    return HealthSnapshot{
        get_pid(),
        now_rfc3339(),
        static_cast<std::uint64_t>(elapsed.count()),
        reg.components
    };
}

nlohmann::json snapshot_json() {
    auto snap = snapshot();
    nlohmann::json j;
    j["pid"] = snap.pid;
    j["updated_at"] = snap.updated_at;
    j["uptime_seconds"] = snap.uptime_seconds;
    j["components"] = nlohmann::json::object();
    for (const auto& [name, health] : snap.components) {
        nlohmann::json cj;
        cj["status"] = health.status;
        cj["updated_at"] = health.updated_at;
        cj["last_ok"] = health.last_ok.has_value() ? health.last_ok.value() : nullptr;
        cj["last_error"] = health.last_error.has_value() ? health.last_error.value() : nullptr;
        cj["restart_count"] = health.restart_count;
        j["components"][name] = cj;
    }
    return j;
}

}
