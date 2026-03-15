#pragma once
/// SOP audit logger — mirrors Rust src/sop/audit.rs

#include "types.hpp"
#include <string>

namespace zeroclaw {
namespace sop {

/// Logs SOP run events for audit trail.
/// Matching Rust SopAuditLogger.
class SopAuditLogger {
public:
    SopAuditLogger() = default;

    /// Log that a SOP run was started
    void log_run_started(const SopRun& run) const;

    /// Log that a SOP step completed
    void log_step_result(const SopRun& run, const SopStep& step,
                          const SopStepResult& result) const;

    /// Log that a SOP run finished
    void log_run_finished(const SopRun& run) const;
};

} // namespace sop
} // namespace zeroclaw
