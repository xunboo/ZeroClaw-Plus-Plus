#include "shell.hpp"
#include <cstdlib>
#include <array>
#include <sstream>
#include <algorithm>
#include <cctype>

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
    const security::SecurityPolicy& /*security*/) {
    std::vector<std::string> result;
    for (const auto& var : SAFE_ENV_VARS) {
        const char* val = std::getenv(var.c_str());
        if (val != nullptr) {
            result.push_back(var);
        }
    }
    return result;
}

ShellTool::ShellTool(std::shared_ptr<security::SecurityPolicy> security)
    : security_(std::move(security)) {}

ToolResult ShellTool::execute(const nlohmann::json& args) {
    // Extract parameters
    if (!args.contains("command") || !args["command"].is_string()) {
        return ToolResult::fail("Missing required parameter: command");
    }

    std::string command = args["command"].get<std::string>();
    if (command.empty()) {
        return ToolResult::fail("Command cannot be empty");
    }

    int timeout = args.value("timeout", DEFAULT_SHELL_TIMEOUT_SECS);
    (void)timeout;  // Used in full implementation

    // In a full implementation, this would:
    // 1. Check security policy for command allowlist/blocklist
    // 2. Sanitize environment variables
    // 3. Run command in sandbox (bubblewrap/firejail/docker)
    // 4. Capture stdout/stderr with timeout
    // 5. Truncate output if too large

    // Synchronous execution for now
    std::string output;
    int exit_code = 0;

#ifdef _WIN32
    // Use _popen on Windows
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        return ToolResult::fail("Failed to execute command: " + command);
    }
    std::array<char, 4096> buffer;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
        if (output.size() > MAX_SHELL_OUTPUT_BYTES) {
            output = output.substr(0, MAX_SHELL_OUTPUT_BYTES) +
                     "\n\n[Output truncated at 1MB]";
            break;
        }
    }
    exit_code = _pclose(pipe);
#else
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return ToolResult::fail("Failed to execute command: " + command);
    }
    std::array<char, 4096> buffer;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
        if (output.size() > MAX_SHELL_OUTPUT_BYTES) {
            output = output.substr(0, MAX_SHELL_OUTPUT_BYTES) +
                     "\n\n[Output truncated at 1MB]";
            break;
        }
    }
    int status = pclose(pipe);
    exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif

    std::string result = "Exit code: " + std::to_string(exit_code) + "\n";
    if (!output.empty()) {
        result += "Output:\n" + output;
    }

    if (exit_code != 0) {
        return ToolResult::fail(result);
    }
    return ToolResult::ok(result);
}

} // namespace tools
} // namespace zeroclaw
