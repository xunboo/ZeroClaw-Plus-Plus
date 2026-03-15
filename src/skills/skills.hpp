#pragma once

/// Skills module — skill discovery, loading, and audit.
/// Skills are markdown-defined capability extensions that can be injected into prompts.

#include <string>
#include <vector>
#include <optional>
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace skills {

/// A loaded skill definition
struct Skill {
    std::string name;
    std::string description;
    std::string content;            ///< Full markdown content
    std::string file_path;          ///< Path to the SKILL.md file
    std::vector<std::string> tags;

    /// Generate prompt injection text for this skill
    std::string prompt_injection() const;
};

/// Skill prompt injection mode
enum class PromptInjectionMode {
    Full,       ///< Inject full skill content
    Summary,    ///< Inject only name + description
    None        ///< No injection
};

/// Load all skills from a workspace directory
std::vector<Skill> load_skills(const std::string& workspace_dir);

/// Load skills with config-based filtering
std::vector<Skill> load_skills_with_config(const std::string& workspace_dir,
                                             const std::vector<std::string>& enabled_skills = {});

/// Audit a skill file for security issues
struct SkillAuditResult {
    bool passed = true;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

SkillAuditResult audit_skill(const std::string& skill_path);

/// Find skill directories in a workspace
std::vector<std::string> find_skill_dirs(const std::string& workspace_dir);

} // namespace skills
} // namespace zeroclaw
