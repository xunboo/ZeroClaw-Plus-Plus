#include "skills.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace zeroclaw {
namespace skills {

std::string Skill::prompt_injection() const {
    return "## Skill: " + name + "\n" + description + "\n\n" + content;
}

std::vector<std::string> find_skill_dirs(const std::string& workspace_dir) {
    std::vector<std::string> dirs;
    const std::vector<std::string> search_dirs = {
        ".agents", ".agent", "_agents", "_agent"
    };
    for (const auto& sd : search_dirs) {
        auto path = fs::path(workspace_dir) / sd;
        if (fs::is_directory(path)) {
            dirs.push_back(path.string());
        }
    }
    return dirs;
}

std::vector<Skill> load_skills(const std::string& workspace_dir) {
    std::vector<Skill> skills;
    auto dirs = find_skill_dirs(workspace_dir);
    for (const auto& dir : dirs) {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.path().filename() == "SKILL.md") {
                std::ifstream file(entry.path());
                if (!file.is_open()) continue;
                std::string content((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
                Skill skill;
                skill.file_path = entry.path().string();
                skill.name = entry.path().parent_path().filename().string();
                skill.content = content;
                // Parse YAML frontmatter for description
                if (content.find("---") == 0) {
                    auto end = content.find("---", 3);
                    if (end != std::string::npos) {
                        auto desc_pos = content.find("description:");
                        if (desc_pos != std::string::npos && desc_pos < end) {
                            auto line_end = content.find('\n', desc_pos);
                            skill.description = content.substr(desc_pos + 13,
                                line_end - desc_pos - 13);
                        }
                        skill.content = content.substr(end + 4);
                    }
                }
                skills.push_back(skill);
            }
        }
    }
    return skills;
}

std::vector<Skill> load_skills_with_config(const std::string& workspace_dir,
                                             const std::vector<std::string>& enabled_skills) {
    auto all = load_skills(workspace_dir);
    if (enabled_skills.empty()) return all;
    std::vector<Skill> filtered;
    for (const auto& s : all) {
        for (const auto& e : enabled_skills) {
            if (s.name == e) { filtered.push_back(s); break; }
        }
    }
    return filtered;
}

SkillAuditResult audit_skill(const std::string& skill_path) {
    SkillAuditResult result;
    if (!fs::exists(skill_path)) {
        result.passed = false;
        result.errors.push_back("Skill file not found: " + skill_path);
    }
    return result;
}

} // namespace skills
} // namespace zeroclaw
