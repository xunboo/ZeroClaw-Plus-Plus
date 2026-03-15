#include "file_write.hpp"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace zeroclaw {
namespace tools {

// ── FileWriteTool ────────────────────────────────────────────────

FileWriteTool::FileWriteTool(std::shared_ptr<security::SecurityPolicy> security)
    : security_(std::move(security)) {}

ToolResult FileWriteTool::execute(const nlohmann::json& args) {
    if (!args.contains("path") || !args["path"].is_string()) {
        return ToolResult::fail("Missing required parameter: path");
    }
    if (!args.contains("content") || !args["content"].is_string()) {
        return ToolResult::fail("Missing required parameter: content");
    }

    std::string path = args["path"].get<std::string>();
    std::string content = args["content"].get<std::string>();

    if (path.empty()) {
        return ToolResult::fail("Path cannot be empty");
    }

    // In full implementation: validate path against security policy
    // (workspace sandboxing, symlink checks, TOCTOU prevention)

    // Create parent directories
    std::error_code ec;
    auto parent = fs::path(path).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            return ToolResult::fail("Failed to create parent directories: " + ec.message());
        }
    }

    // Write file
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return ToolResult::fail("Cannot open file for writing: " + path);
    }
    file << content;
    file.close();

    if (file.fail()) {
        return ToolResult::fail("Failed to write file: " + path);
    }

    return ToolResult::ok("Successfully wrote " + std::to_string(content.size()) +
                                " bytes to " + path);
}

// ── FileEditTool ─────────────────────────────────────────────────

FileEditTool::FileEditTool(std::shared_ptr<security::SecurityPolicy> security)
    : security_(std::move(security)) {}

ToolResult FileEditTool::execute(const nlohmann::json& args) {
    if (!args.contains("path") || !args["path"].is_string()) {
        return ToolResult::fail("Missing required parameter: path");
    }
    if (!args.contains("old_text") || !args["old_text"].is_string()) {
        return ToolResult::fail("Missing required parameter: old_text");
    }
    if (!args.contains("new_text") || !args["new_text"].is_string()) {
        return ToolResult::fail("Missing required parameter: new_text");
    }

    std::string path = args["path"].get<std::string>();
    std::string old_text = args["old_text"].get<std::string>();
    std::string new_text = args["new_text"].get<std::string>();

    // Read existing file
    std::ifstream in_file(path, std::ios::binary);
    if (!in_file.is_open()) {
        return ToolResult::fail("File not found: " + path);
    }
    std::string content((std::istreambuf_iterator<char>(in_file)),
                         std::istreambuf_iterator<char>());
    in_file.close();

    // Find and replace
    auto pos = content.find(old_text);
    if (pos == std::string::npos) {
        return ToolResult::fail("old_text not found in file. "
                                  "The text must match exactly, including whitespace.");
    }

    // Check for multiple occurrences
    auto second_pos = content.find(old_text, pos + old_text.size());
    if (second_pos != std::string::npos) {
        return ToolResult::fail("old_text found multiple times in file. "
                                  "Please provide more context to make the match unique.");
    }

    content.replace(pos, old_text.size(), new_text);

    // Write back
    std::ofstream out_file(path, std::ios::binary);
    if (!out_file.is_open()) {
        return ToolResult::fail("Cannot open file for writing: " + path);
    }
    out_file << content;
    out_file.close();

    return ToolResult::ok("Successfully edited " + path);
}

} // namespace tools
} // namespace zeroclaw
