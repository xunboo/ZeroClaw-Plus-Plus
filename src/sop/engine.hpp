#pragma once
/// SOP execution engine — mirrors Rust src/sop/engine.rs

#include "types.hpp"
#include "audit.hpp"
#include <vector>
#include <functional>

namespace zeroclaw {
namespace sop {

/// Callback type for requesting human confirmation of a step
using ConfirmationCallback = std::function<bool(const Sop&, const SopStep&)>;

/// SOP execution engine — runs SOP procedures step by step.
/// Mirrors Rust SopEngine.
class SopEngine {
public:
    explicit SopEngine(SopAuditLogger audit_logger = {});

    /// Execute a SOP from start to finish.
    /// Returns the completed SopRun record.
    SopRun execute(const Sop& sop,
                   const std::string& run_id,
                   const ConfirmationCallback& confirm = nullptr);

private:
    SopAuditLogger audit_;

    /// Execute a single step, returning the step result
    SopStepResult execute_step(const Sop& sop, const SopStep& step,
                                const ConfirmationCallback& confirm);
};

} // namespace sop
} // namespace zeroclaw
