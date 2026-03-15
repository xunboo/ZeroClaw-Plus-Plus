#include "audit.hpp"
#include <cstdio>

namespace zeroclaw {
namespace sop {

void SopAuditLogger::log_run_started(const SopRun& run) const {
    std::fprintf(stderr, "[SOP AUDIT] run_started sop=%s run_id=%s\n",
                 run.sop_name.c_str(), run.id.c_str());
}

void SopAuditLogger::log_step_result(const SopRun& run, const SopStep& step,
                                      const SopStepResult& result) const {
    const char* status_str = "unknown";
    switch (result.status) {
        case SopStepStatus::Pending: status_str = "pending"; break;
        case SopStepStatus::Running: status_str = "running"; break;
        case SopStepStatus::Success: status_str = "success"; break;
        case SopStepStatus::Failed:  status_str = "failed";  break;
        case SopStepStatus::Skipped: status_str = "skipped"; break;
    }
    std::fprintf(stderr, "[SOP AUDIT] step_result sop=%s run_id=%s step=%u title=%s status=%s\n",
                 run.sop_name.c_str(), run.id.c_str(),
                 step.number, step.title.c_str(), status_str);
}

void SopAuditLogger::log_run_finished(const SopRun& run) const {
    const char* status_str = "unknown";
    switch (run.status) {
        case SopRunStatus::Running:   status_str = "running";   break;
        case SopRunStatus::Completed: status_str = "completed"; break;
        case SopRunStatus::Failed:    status_str = "failed";    break;
        case SopRunStatus::Cancelled: status_str = "cancelled"; break;
    }
    std::fprintf(stderr, "[SOP AUDIT] run_finished sop=%s run_id=%s status=%s\n",
                 run.sop_name.c_str(), run.id.c_str(), status_str);
}

} // namespace sop
} // namespace zeroclaw
