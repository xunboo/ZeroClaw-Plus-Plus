#include "shell.hpp"
#include "../security/policy.hpp"
#include <cstdlib>
#include <array>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace zeroclaw {
namespace tools {

bool is_valid_env_var_name(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return !std::isdigit(static_cast<unsigned char>(name[0]));
}

std::vector<std::string> collect_allowed_shell_env_vars(
    const security::SecurityPolicy& security) {
    std::vector<std::string> result;
    std::vector<std::string> candidate_vars = SAFE_ENV_VARS;
    // Append any user-configured passthrough vars
    for (const auto& extra : security.shell_env_passthrough) {
        candidate_vars.push_back(extra);
    }

    // Deduplicate and validate
    std::vector<std::string> seen;
    for (const auto& var : candidate_vars) {
        std::string trimmed = var;
        // trim whitespace
        auto start = trimmed.find_first_not_of(" \t");
        auto end   = trimmed.find_last_not_of(" \t");
        if (start == std::string::npos) continue;
        trimmed = trimmed.substr(start, end - start + 1);

        if (!is_valid_env_var_name(trimmed)) continue;
        if (std::find(seen.begin(), seen.end(), trimmed) != seen.end()) continue;
        seen.push_back(trimmed);
        result.push_back(trimmed);
    }
    return result;
}

ShellTool::ShellTool(std::shared_ptr<security::SecurityPolicy> security)
    : security_(std::move(security)) {}

ToolResult ShellTool::execute(const nlohmann::json& args) {
    // --- Parameter extraction ---
    if (!args.contains("command") || !args["command"].is_string()) {
        return ToolResult::fail("Missing required parameter: command");
    }
    std::string command = args["command"].get<std::string>();
    if (command.empty()) {
        return ToolResult::fail("Command cannot be empty");
    }
    bool approved = args.value("approved", false);
    int timeout_secs = args.value("timeout", DEFAULT_SHELL_TIMEOUT_SECS);

    // --- Security: rate limit pre-check ---
    if (security_->is_rate_limited()) {
        return ToolResult::fail("Rate limit exceeded: too many actions in the last hour");
    }

    // --- Security: validate command using policy (allowlist + risk gate) ---
    auto validation = security_->validate_command_execution(command, approved);
    if (!validation.allowed) {
        return ToolResult::fail(validation.error);
    }

    // --- Security: check for forbidden path arguments ---
    std::string forbidden = security_->forbidden_path_argument(command);
    if (!forbidden.empty()) {
        return ToolResult::fail("Path blocked by security policy: " + forbidden);
    }

    // --- Rate limit: consume one action token ---
    if (!security_->record_action()) {
        return ToolResult::fail("Rate limit exceeded: action budget exhausted");
    }

    // --- Build environment: clear and re-add only safe vars ---
    std::vector<std::string> env_vars = collect_allowed_shell_env_vars(*security_);

    // --- Execute with timeout ---
    std::string output;
    int exit_code = 0;
    bool timed_out = false;

    // Build the full command with cd to workspace if it exists
    std::string workspace = security_->workspace_dir.string();
    std::string full_cmd;
    if (!workspace.empty() && workspace != ".") {
#ifdef _WIN32
        full_cmd = "cd /d \"" + workspace + "\" && " + command + " 2>&1";
#else
        full_cmd = "cd \"" + workspace + "\" && " + command + " 2>&1";
#endif
    } else {
        full_cmd = command + " 2>&1";
    }

#ifdef _WIN32
    FILE* pipe = _popen(full_cmd.c_str(), "r");
#else
    FILE* pipe = popen(full_cmd.c_str(), "r");
#endif

    if (!pipe) {
        return ToolResult::fail("Failed to execute command: " + command);
    }

    std::array<char, 4096> buffer;
    auto start_time = std::chrono::steady_clock::now();

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
        if (output.size() > MAX_SHELL_OUTPUT_BYTES) {
            // Find a safe truncation point at byte boundary
            output.resize(MAX_SHELL_OUTPUT_BYTES);
            output += "\n... [output truncated at 1MB]";
            timed_out = false;
            break;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time);
        if (elapsed.count() >= timeout_secs) {
            timed_out = true;
            break;
        }
    }

#ifdef _WIN32
    exit_code = _pclose(pipe);
#else
    int status = pclose(pipe);
    exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif

    if (timed_out) {
        return ToolResult::fail("Command timed out after " + std::to_string(timeout_secs) +
                                "s and was killed");
    }

    bool success = (exit_code == 0);
    if (!success) {
        return ToolResult{false, output, "Exit code: " + std::to_string(exit_code)};
    }
    return ToolResult::ok(output);
}

} // namespace tools
} // namespace zeroclaw
