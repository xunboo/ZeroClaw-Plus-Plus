#pragma once

/// SkillForge module — automated skill generation pipeline.
/// Scout discovers patterns, Evaluate tests them, Integrate commits to workspace.

#include <string>
#include <vector>
#include <optional>
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace skillforge {

/// A skill candidate discovered by the scout
struct SkillCandidate {
    std::string name;
    std::string description;
    std::string content;
    double confidence = 0.0;
    std::string source;    ///< what triggered discovery
};

/// Evaluation result for a skill candidate
struct SkillEvaluation {
    bool passed = false;
    double quality_score = 0.0;
    std::vector<std::string> issues;
    std::string improved_content;  ///< auto-improved version if applicable
};

/// Scout — discovers skill patterns from conversation history and agent behavior
class Scout {
public:
    /// Scan conversation history for potential skill patterns
    std::vector<SkillCandidate> scan(const std::string& workspace_dir,
                                      const std::vector<std::string>& conversation_logs = {});

    /// Suggest skills based on repeated user requests
    std::vector<SkillCandidate> suggest_from_patterns(
        const std::vector<std::pair<std::string, int>>& command_frequencies);
};

/// Evaluate — test and score skill candidates
class Evaluate {
public:
    /// Evaluate a skill candidate for quality, safety, and usefulness
    SkillEvaluation evaluate(const SkillCandidate& candidate);

    /// Batch evaluate multiple candidates
    std::vector<SkillEvaluation> batch_evaluate(const std::vector<SkillCandidate>& candidates);
};

/// Integrate — commit approved skills to the workspace
class Integrate {
public:
    /// Write an approved skill to the workspace skill directory
    bool commit_skill(const std::string& workspace_dir,
                       const SkillCandidate& candidate,
                       const std::string& skill_dir_name = ".agents/skills");

    /// Remove a skill from the workspace
    bool remove_skill(const std::string& workspace_dir,
                       const std::string& skill_name,
                       const std::string& skill_dir_name = ".agents/skills");
};

} // namespace skillforge
} // namespace zeroclaw
