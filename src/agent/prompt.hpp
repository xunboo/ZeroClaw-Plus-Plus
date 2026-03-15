#pragma once

/// System prompt builder — assembles the system prompt from modular sections.

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <ctime>
#include "../tools/traits.hpp"

namespace zeroclaw {
namespace agent {

/// Maximum characters to read from workspace bootstrap files
static constexpr size_t BOOTSTRAP_MAX_CHARS = 20000;

/// Context provided to prompt sections for building
struct PromptContext {
    std::filesystem::path workspace_dir;
    std::string model_name;
    const std::vector<std::unique_ptr<Tool>>* tools = nullptr;
    std::string dispatcher_instructions;
};

/// Abstract prompt section — each section builds a part of the system prompt
class PromptSection {
public:
    virtual ~PromptSection() = default;
    virtual std::string name() const = 0;
    virtual std::string build(const PromptContext& ctx) const = 0;
};

/// Injects a workspace file into the prompt string
inline void inject_workspace_file(std::string& prompt,
                                   const std::filesystem::path& workspace_dir,
                                   const std::string& filename) {
    auto path = workspace_dir / filename;
    std::ifstream ifs(path);
    if (ifs.is_open()) {
        std::string content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
        // Trim
        size_t s = content.find_first_not_of(" \t\n\r");
        size_t e = content.find_last_not_of(" \t\n\r");
        if (s == std::string::npos) return;
        std::string trimmed = content.substr(s, e - s + 1);

        prompt += "### " + filename + "\n\n";
        if (trimmed.size() > BOOTSTRAP_MAX_CHARS) {
            prompt += trimmed.substr(0, BOOTSTRAP_MAX_CHARS);
            prompt += "\n\n[... truncated at " + std::to_string(BOOTSTRAP_MAX_CHARS)
                   + " chars — use `read` for full file]\n\n";
        } else {
            prompt += trimmed + "\n\n";
        }
    } else {
        prompt += "### " + filename + "\n\n[File not found: " + filename + "]\n\n";
    }
}

/// Identity section — loads workspace identity files
class IdentitySection : public PromptSection {
public:
    std::string name() const override { return "identity"; }
    std::string build(const PromptContext& ctx) const override {
        std::string prompt = "## Project Context\n\n";
        prompt += "The following workspace files define your identity, behavior, and context.\n\n";
        for (const auto& file : {"AGENTS.md", "SOUL.md", "TOOLS.md", "IDENTITY.md",
                                  "USER.md", "HEARTBEAT.md", "BOOTSTRAP.md", "MEMORY.md"}) {
            inject_workspace_file(prompt, ctx.workspace_dir, file);
        }
        return prompt;
    }
};

/// Tools section — lists available tools and dispatcher instructions
class ToolsSection : public PromptSection {
public:
    std::string name() const override { return "tools"; }
    std::string build(const PromptContext& ctx) const override {
        std::ostringstream oss;
        oss << "## Tools\n\n";
        if (ctx.tools) {
            for (const auto& tool : *ctx.tools) {
                oss << "- **" << tool->name() << "**: " << tool->description()
                    << "\n  Parameters: `" << tool->parameters_schema().dump() << "`\n";
            }
        }
        if (!ctx.dispatcher_instructions.empty()) {
            oss << "\n" << ctx.dispatcher_instructions;
        }
        return oss.str();
    }
};

/// Safety section — security guidelines
class SafetySection : public PromptSection {
public:
    std::string name() const override { return "safety"; }
    std::string build(const PromptContext& /*ctx*/) const override {
        return "## Safety\n\n"
               "- Do not exfiltrate private data.\n"
               "- Do not run destructive commands without asking.\n"
               "- Do not bypass oversight or approval mechanisms.\n"
               "- Prefer `trash` over `rm`.\n"
               "- When in doubt, ask before acting externally.";
    }
};

/// Workspace section — shows the working directory
class WorkspaceSection : public PromptSection {
public:
    std::string name() const override { return "workspace"; }
    std::string build(const PromptContext& ctx) const override {
        return "## Workspace\n\nWorking directory: `" + ctx.workspace_dir.string() + "`";
    }
};

/// DateTime section — current date and time
class DateTimeSection : public PromptSection {
public:
    std::string name() const override { return "datetime"; }
    std::string build(const PromptContext& /*ctx*/) const override {
        auto now = std::time(nullptr);
        auto* tm = std::localtime(&now);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
        char tz[16];
        std::strftime(tz, sizeof(tz), "%Z", tm);
        return std::string("## Current Date & Time\n\n") + buf + " (" + tz + ")";
    }
};

/// Runtime section — host/OS/model info
class RuntimeSection : public PromptSection {
public:
    std::string name() const override { return "runtime"; }
    std::string build(const PromptContext& ctx) const override {
#ifdef _WIN32
        const char* os_name = "windows";
#elif defined(__linux__)
        const char* os_name = "linux";
#elif defined(__APPLE__)
        const char* os_name = "macos";
#else
        const char* os_name = "unknown";
#endif
        return "## Runtime\n\nOS: " + std::string(os_name) + " | Model: " + ctx.model_name;
    }
};

/// Assembles all prompt sections into a final system prompt
class SystemPromptBuilder {
public:
    /// Create builder with default sections
    static SystemPromptBuilder with_defaults() {
        SystemPromptBuilder builder;
        builder.sections_.push_back(std::make_unique<IdentitySection>());
        builder.sections_.push_back(std::make_unique<ToolsSection>());
        builder.sections_.push_back(std::make_unique<SafetySection>());
        builder.sections_.push_back(std::make_unique<WorkspaceSection>());
        builder.sections_.push_back(std::make_unique<DateTimeSection>());
        builder.sections_.push_back(std::make_unique<RuntimeSection>());
        return builder;
    }

    /// Add a custom section
    void add_section(std::unique_ptr<PromptSection> section) {
        sections_.push_back(std::move(section));
    }

    /// Build the complete system prompt
    std::string build(const PromptContext& ctx) const {
        std::string output;
        for (const auto& section : sections_) {
            auto part = section->build(ctx);
            // Trim trailing whitespace
            size_t end = part.find_last_not_of(" \t\n\r");
            if (end == std::string::npos) continue;
            output += part.substr(0, end + 1) + "\n\n";
        }
        return output;
    }

private:
    std::vector<std::unique_ptr<PromptSection>> sections_;
};

} // namespace agent
} // namespace zeroclaw
