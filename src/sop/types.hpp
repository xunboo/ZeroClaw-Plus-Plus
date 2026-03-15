#pragma once
/// SOP (Standard Operating Procedure) data types.
/// Mirrors Rust src/sop/types.rs

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <variant>

namespace zeroclaw {
namespace sop {

// ── Execution mode ──────────────────────────────────────────────

/// How a SOP step is executed — matching Rust SopExecutionMode
enum class SopExecutionMode {
    Supervised,  // Each step requires human confirmation
    Auto,        // Steps execute automatically
};

// ── Priority ────────────────────────────────────────────────────

/// Matching Rust SopPriority
enum class SopPriority {
    Low,
    Normal,
    High,
    Critical,
};

// ── Trigger ─────────────────────────────────────────────────────

struct SopTriggerManual {};

struct SopTriggerWebhook {
    std::string path;
};

struct SopTriggerCron {
    std::string expression;
};

struct SopTriggerMqtt {
    std::string topic;
    std::optional<std::string> condition;
};

struct SopTriggerPeripheral {
    std::string board;
    std::string signal;
    std::optional<std::string> condition;
};

using SopTrigger = std::variant<
    SopTriggerManual,
    SopTriggerWebhook,
    SopTriggerCron,
    SopTriggerMqtt,
    SopTriggerPeripheral
>;

// ── Step ────────────────────────────────────────────────────────

/// A single step in a SOP procedure — matching Rust SopStep
struct SopStep {
    uint32_t number = 0;
    std::string title;
    std::string body;
    std::vector<std::string> suggested_tools;
    bool requires_confirmation = false;
};

// ── SOP ─────────────────────────────────────────────────────────

/// A Standard Operating Procedure — matching Rust Sop
struct Sop {
    std::string name;
    std::string description;
    std::string version;
    SopPriority priority = SopPriority::Normal;
    SopExecutionMode execution_mode = SopExecutionMode::Supervised;
    std::vector<SopTrigger> triggers;
    std::vector<SopStep> steps;
    uint64_t cooldown_secs = 0;
    uint32_t max_concurrent = 1;
    std::optional<std::filesystem::path> location;
};

// ── Run status ──────────────────────────────────────────────────

enum class SopStepStatus {
    Pending,
    Running,
    Success,
    Failed,
    Skipped,
};

struct SopStepResult {
    SopStepStatus status = SopStepStatus::Pending;
    std::string output;
};

enum class SopRunStatus {
    Running,
    Completed,
    Failed,
    Cancelled,
};

/// A SOP execution run — matching Rust SopRun
struct SopRun {
    std::string id;
    std::string sop_name;
    SopRunStatus status = SopRunStatus::Running;
    std::vector<SopStepResult> step_results;
    std::string started_at;
    std::optional<std::string> finished_at;
};

} // namespace sop
} // namespace zeroclaw
