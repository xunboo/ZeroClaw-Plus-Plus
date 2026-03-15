#pragma once

/// File write tool — write file contents with path sandboxing.

#include <string>
#include <memory>
#include "traits.hpp"
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace security { class SecurityPolicy; }
namespace tools {

/// File write tool with path sandboxing and parent directory creation
class FileWriteTool : public Tool {
public:
    explicit FileWriteTool(std::shared_ptr<security::SecurityPolicy> security);

    std::string name() const override { return "file_write"; }
    std::string description() const override {
        return "Write content to a file at the given path. Creates parent directories "
               "if they don't exist. Paths are sandboxed to the workspace directory.";
    }
    nlohmann::json parameters_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "File path relative to workspace"}}},
                {"content", {{"type", "string"}, {"description", "Content to write to the file"}}}
            }},
            {"required", nlohmann::json::array({"path", "content"})}
        };
    }
    ToolResult execute(const nlohmann::json& args) override;

private:
    std::shared_ptr<security::SecurityPolicy> security_;
};

/// File edit tool — apply targeted edits to existing files
class FileEditTool : public Tool {
public:
    explicit FileEditTool(std::shared_ptr<security::SecurityPolicy> security);

    std::string name() const override { return "file_edit"; }
    std::string description() const override {
        return "Apply targeted edits to an existing file. Specify the old text to find "
               "and the new text to replace it with. The old text must match exactly.";
    }
    nlohmann::json parameters_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "File path relative to workspace"}}},
                {"old_text", {{"type", "string"}, {"description", "Exact text to find and replace"}}},
                {"new_text", {{"type", "string"}, {"description", "Replacement text"}}}
            }},
            {"required", nlohmann::json::array({"path", "old_text", "new_text"})}
        };
    }
    ToolResult execute(const nlohmann::json& args) override;

private:
    std::shared_ptr<security::SecurityPolicy> security_;
};

} // namespace tools
} // namespace zeroclaw
