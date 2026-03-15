#include "skillforge.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace zeroclaw {
namespace skillforge {

std::vector<SkillCandidate> Scout::scan(const std::string& /*workspace_dir*/,
                                          const std::vector<std::string>& /*conversation_logs*/) {
    return {};
}

std::vector<SkillCandidate> Scout::suggest_from_patterns(
    const std::vector<std::pair<std::string, int>>& /*command_frequencies*/) {
    return {};
}

SkillEvaluation Evaluate::evaluate(const SkillCandidate& candidate) {
    SkillEvaluation eval;
    eval.passed = !candidate.content.empty() && candidate.confidence > 0.5;
    eval.quality_score = candidate.confidence;
    if (candidate.name.empty()) {
        eval.issues.push_back("Skill name is empty");
        eval.passed = false;
    }
    return eval;
}

std::vector<SkillEvaluation> Evaluate::batch_evaluate(
    const std::vector<SkillCandidate>& candidates) {
    std::vector<SkillEvaluation> results;
    for (const auto& c : candidates) results.push_back(evaluate(c));
    return results;
}

bool Integrate::commit_skill(const std::string& workspace_dir,
                               const SkillCandidate& candidate,
                               const std::string& skill_dir_name) {
    auto skill_dir = fs::path(workspace_dir) / skill_dir_name / candidate.name;
    std::error_code ec;
    fs::create_directories(skill_dir, ec);
    if (ec) return false;

    std::ofstream file(skill_dir / "SKILL.md");
    if (!file.is_open()) return false;
    file << "---\ndescription: " << candidate.description << "\n---\n\n"
         << candidate.content;
    return true;
}

bool Integrate::remove_skill(const std::string& workspace_dir,
                               const std::string& skill_name,
                               const std::string& skill_dir_name) {
    auto skill_dir = fs::path(workspace_dir) / skill_dir_name / skill_name;
    std::error_code ec;
    return fs::remove_all(skill_dir, ec) > 0;
}

} // namespace skillforge
} // namespace zeroclaw
