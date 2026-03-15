#include "file_read.hpp"
#include "../security/policy.hpp"
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

    // --- Security: rate limit pre-check ---
    if (security_->is_rate_limited()) {
        return ToolResult::fail("Rate limit exceeded: too many actions in the last hour");
    }

    // --- Security: validate path is within workspace (pre-canonicalization) ---
    if (!security_->is_path_allowed(path)) {
        return ToolResult::fail("Path not allowed by security policy: " + path);
    }

    // --- Rate limit: consume one action token (before canonicalization, to prevent
    // existence-probing attacks where callers enumerate paths for free) ---
    if (!security_->record_action()) {
        return ToolResult::fail("Rate limit exceeded: action budget exhausted");
    }

    // --- Resolve workspace-relative paths ---
    fs::path full_path;
    if (fs::path(path).is_absolute()) {
        full_path = path;
    } else {
        full_path = security_->workspace_dir / path;
    }

    // --- Security: resolve canonicalized path to block symlink escapes ---
    std::error_code ec;
    fs::path resolved_path = fs::canonical(full_path, ec);
    if (ec) {
        return ToolResult::fail("Failed to resolve file path: " + ec.message());
    }

    if (!security_->is_resolved_path_allowed(resolved_path)) {
        return ToolResult::fail(security_->resolved_path_violation_message(resolved_path));
    }

    // --- Check file size AFTER canonicalization (prevent TOCTOU symlink bypass) ---
    auto file_size = fs::file_size(resolved_path, ec);
    if (ec) {
        return ToolResult::fail("Cannot determine file size: " + ec.message());
    }
    if (file_size > MAX_FILE_SIZE_BYTES) {
        return ToolResult::fail("File too large: " + std::to_string(file_size) +
                                " bytes (limit: " + std::to_string(MAX_FILE_SIZE_BYTES) + " bytes)");
    }

    // --- Read file as text ---
    std::ifstream file(resolved_path, std::ios::in);
    if (!file.is_open()) {
        return ToolResult::fail("Cannot open file: " + resolved_path.string());
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // --- Split into lines ---
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        // Remove trailing \r for Windows-style line endings
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }

    size_t total = lines.size();

    if (total == 0) {
        return ToolResult::ok("");
    }

    // --- Apply offset (1-based in Rust, convert to 0-based) ---
    size_t start = 0;
    if (args.contains("offset") && args["offset"].is_number()) {
        int64_t raw_offset = args["offset"].get<int64_t>();
        if (raw_offset >= 1) {
            start = static_cast<size_t>(raw_offset) - 1;
        }
    }
    start = std::min(start, total);

    // --- Apply limit ---
    size_t end = total;
    if (args.contains("limit") && args["limit"].is_number()) {
        int64_t raw_limit = args["limit"].get<int64_t>();
        if (raw_limit > 0) {
            size_t limit = static_cast<size_t>(raw_limit);
            end = std::min(start + limit, total);
        }
    }

    if (start >= end) {
        return ToolResult::ok("[No lines in range, file has " + std::to_string(total) + " lines]");
    }

    // --- Format output with line numbers ---
    std::string result;
    for (size_t i = start; i < end; ++i) {
        result += std::to_string(i + 1) + ": " + lines[i] + "\n";
    }

    // --- Append range summary ---
    bool partial = (start > 0 || end < total);
    if (partial) {
        result += "[Lines " + std::to_string(start + 1) + "-" + std::to_string(end) +
                  " of " + std::to_string(total) + "]";
    } else {
        result += "[" + std::to_string(total) + " lines total]";
    }

    return ToolResult::ok(result);
}

} // namespace tools
} // namespace zeroclaw
