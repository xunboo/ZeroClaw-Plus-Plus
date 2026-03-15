#include "file_read.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace zeroclaw {
namespace tools {

FileReadTool::FileReadTool(std::shared_ptr<security::SecurityPolicy> security)
    : security_(std::move(security)) {}

ToolResult FileReadTool::execute(const nlohmann::json& args) {
    if (!args.contains("path") || !args["path"].is_string()) {
        return ToolResult::fail("Missing required parameter: path");
    }

    std::string path = args["path"].get<std::string>();
    if (path.empty()) {
        return ToolResult::fail("Path cannot be empty");
    }

    // In full implementation: validate path against security policy
    // (workspace sandboxing, symlink checks, path traversal prevention)

    // Check file exists
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return ToolResult::fail("File not found: " + path);
    }

    // Check file size
    auto file_size = fs::file_size(path, ec);
    if (ec) {
        return ToolResult::fail("Cannot determine file size: " + path);
    }
    if (file_size > MAX_FILE_SIZE_BYTES) {
        return ToolResult::fail("File too large (" + std::to_string(file_size) +
                                 " bytes, max " + std::to_string(MAX_FILE_SIZE_BYTES) + ")");
    }

    // Read file
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return ToolResult::fail("Cannot open file: " + path);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // Handle offset and limit
    int offset = args.value("offset", 0);
    int limit = args.value("limit", -1);

    if (offset > 0 || limit > 0) {
        std::istringstream stream(content);
        std::string line;
        std::string result;
        int line_num = 0;
        int lines_read = 0;

        while (std::getline(stream, line)) {
            if (line_num >= offset) {
                if (limit > 0 && lines_read >= limit) break;
                result += line + "\n";
                ++lines_read;
            }
            ++line_num;
        }
        content = result;
    }

    return ToolResult::ok(content);
}

} // namespace tools
} // namespace zeroclaw
