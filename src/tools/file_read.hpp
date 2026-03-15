#pragma once

/// File read tool — read file contents with path sandboxing.

#include <string>
#include <memory>
#include "traits.hpp"
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace security { class SecurityPolicy; }
namespace tools {

/// Maximum file size for reading (10 MB)
static constexpr uint64_t MAX_FILE_SIZE_BYTES = 10 * 1024 * 1024;

/// File read tool with path sandboxing and binary/PDF support
class FileReadTool : public Tool {
public:
    explicit FileReadTool(std::shared_ptr<security::SecurityPolicy> security);

    std::string name() const override { return "file_read"; }

    std::string description() const override {
        return "Read the contents of a file at the given path. "
               "Supports text files, binary files (lossy), and PDF extraction. "
               "Paths are sandboxed to the workspace directory.";
    }

    nlohmann::json parameters_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"path", {
                    {"type", "string"},
                    {"description", "File path relative to workspace"}
                }},
                {"offset", {
                    {"type", "integer"},
                    {"description", "Line offset to start reading from (0-based)"}
                }},
                {"limit", {
                    {"type", "integer"},
                    {"description", "Maximum number of lines to read"}
                }}
            }},
            {"required", nlohmann::json::array({"path"})}
        };
    }

    ToolResult execute(const nlohmann::json& args) override;

private:
    std::shared_ptr<security::SecurityPolicy> security_;
};

} // namespace tools
} // namespace zeroclaw
