#include "engine.hpp"
#include <chrono>
#include <cstdio>

namespace zeroclaw {
namespace sop {

SopEngine::SopEngine(SopAuditLogger audit_logger)
    : audit_(std::move(audit_logger)) {}

SopRun SopEngine::execute(const Sop& sop,
                           const std::string& run_id,
                           const ConfirmationCallback& confirm) {
    SopRun run;
    run.id = run_id;
    run.sop_name = sop.name;
    run.status = SopRunStatus::Running;

    // Record start time as ISO-formatted string
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    run.started_at = buf;

    audit_.log_run_started(run);

    bool any_failed = false;
    for (const auto& step : sop.steps) {
        auto result = execute_step(sop, step, confirm);
        run.step_results.push_back(result);
        audit_.log_step_result(run, step, result);

        if (result.status == SopStepStatus::Failed) {
            any_failed = true;
            break;
        }
    }

    run.status = any_failed ? SopRunStatus::Failed : SopRunStatus::Completed;

    // Record finish time
    now = std::chrono::system_clock::now();
    t = std::chrono::system_clock::to_time_t(now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    run.finished_at = buf;

    audit_.log_run_finished(run);
    return run;
}

SopStepResult SopEngine::execute_step(const Sop& sop,
                                       const SopStep& step,
                                       const ConfirmationCallback& confirm) {
    SopStepResult result;
    result.status = SopStepStatus::Running;

    // In Supervised mode, ask for confirmation before each step that requires it
    if (step.requires_confirmation && confirm) {
        bool approved = confirm(sop, step);
        if (!approved) {
            result.status = SopStepStatus::Skipped;
            result.output = "Skipped: user did not confirm";
            return result;
        }
    }

    std::fprintf(stderr, "[SOP] Running step %u: %s\n", step.number, step.title.c_str());

    // The actual step execution is handled by the agent via tool dispatch.
    // SopEngine records results; the caller (agent loop) drives execution.
    result.status = SopStepStatus::Success;
    result.output = "Step queued for agent execution";
    return result;
}

} // namespace sop
} // namespace zeroclaw
