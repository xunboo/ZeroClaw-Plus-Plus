#include "policy.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace zeroclaw {
namespace security {

// ── Path helpers ─────────────────────────────────────────────────

std::filesystem::path home_dir() {
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
    if (!home) home = std::getenv("HOME");
#else
    const char* home = std::getenv("HOME");
#endif
    if (home) return std::filesystem::path(home);
    return {};
}

std::filesystem::path expand_user_path(const std::string& path) {
    if (path == "~") {
        auto h = home_dir();
        if (!h.empty()) return h;
    }
    if (path.length() > 2 && path.substr(0, 2) == "~/") {
        auto h = home_dir();
        if (!h.empty()) return h / path.substr(2);
    }
    return std::filesystem::path(path);
}

// ── Shell parsing helpers ────────────────────────────────────────

std::string skip_env_assignments(const std::string& s) {
    std::string rest = s;
    while (true) {
        // Find first non-whitespace
        size_t start = rest.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return rest;

        // Find end of first word
        size_t end = rest.find_first_of(" \t\n\r", start);
        std::string word = (end == std::string::npos) ? rest.substr(start) : rest.substr(start, end - start);

        // Check if it's an env assignment: contains '=' and starts with letter or '_'
        if (word.find('=') != std::string::npos && !word.empty() &&
            (std::isalpha(static_cast<unsigned char>(word[0])) || word[0] == '_')) {
            if (end == std::string::npos) return "";
            rest = rest.substr(end);
            // Trim leading whitespace
            size_t ns = rest.find_first_not_of(" \t\n\r");
            if (ns == std::string::npos) return "";
            rest = rest.substr(ns);
        } else {
            return rest;
        }
    }
}

std::vector<std::string> split_unquoted_segments(const std::string& command) {
    std::vector<std::string> segments;
    std::string current;
    QuoteState quote = QuoteState::None;
    bool escaped = false;

    auto push_segment = [&]() {
        // Trim
        size_t start = current.find_first_not_of(" \t\n\r");
        size_t end = current.find_last_not_of(" \t\n\r");
        if (start != std::string::npos) {
            segments.push_back(current.substr(start, end - start + 1));
        }
        current.clear();
    };

    for (size_t i = 0; i < command.size(); ++i) {
        char ch = command[i];

        switch (quote) {
            case QuoteState::Single:
                if (ch == '\'') quote = QuoteState::None;
                current += ch;
                break;

            case QuoteState::Double:
                if (escaped) { escaped = false; current += ch; continue; }
                if (ch == '\\') { escaped = true; current += ch; continue; }
                if (ch == '"') quote = QuoteState::None;
                current += ch;
                break;

            case QuoteState::None:
                if (escaped) { escaped = false; current += ch; continue; }
                if (ch == '\\') { escaped = true; current += ch; continue; }
                if (ch == '\'') { quote = QuoteState::Single; current += ch; break; }
                if (ch == '"') { quote = QuoteState::Double; current += ch; break; }
                if (ch == ';' || ch == '\n') { push_segment(); break; }
                if (ch == '|') {
                    if (i + 1 < command.size() && command[i + 1] == '|') ++i; // consume ||
                    push_segment();
                    break;
                }
                if (ch == '&') {
                    if (i + 1 < command.size() && command[i + 1] == '&') {
                        ++i; // consume &&
                        push_segment();
                    } else {
                        current += ch;
                    }
                    break;
                }
                current += ch;
                break;
        }
    }

    // Push remaining
    size_t start = current.find_first_not_of(" \t\n\r");
    size_t end = current.find_last_not_of(" \t\n\r");
    if (start != std::string::npos) {
        segments.push_back(current.substr(start, end - start + 1));
    }

    return segments;
}

bool contains_unquoted_single_ampersand(const std::string& command) {
    QuoteState quote = QuoteState::None;
    bool escaped = false;

    for (size_t i = 0; i < command.size(); ++i) {
        char ch = command[i];
        switch (quote) {
            case QuoteState::Single:
                if (ch == '\'') quote = QuoteState::None;
                break;
            case QuoteState::Double:
                if (escaped) { escaped = false; continue; }
                if (ch == '\\') { escaped = true; continue; }
                if (ch == '"') quote = QuoteState::None;
                break;
            case QuoteState::None:
                if (escaped) { escaped = false; continue; }
                if (ch == '\\') { escaped = true; continue; }
                if (ch == '\'') { quote = QuoteState::Single; break; }
                if (ch == '"') { quote = QuoteState::Double; break; }
                if (ch == '&') {
                    if (i + 1 < command.size() && command[i + 1] == '&') {
                        ++i; // skip &&
                    } else {
                        return true; // single &
                    }
                }
                break;
        }
    }
    return false;
}

bool contains_unquoted_char(const std::string& command, char target) {
    QuoteState quote = QuoteState::None;
    bool escaped = false;

    for (char ch : command) {
        switch (quote) {
            case QuoteState::Single:
                if (ch == '\'') quote = QuoteState::None;
                break;
            case QuoteState::Double:
                if (escaped) { escaped = false; continue; }
                if (ch == '\\') { escaped = true; continue; }
                if (ch == '"') quote = QuoteState::None;
                break;
            case QuoteState::None:
                if (escaped) { escaped = false; continue; }
                if (ch == '\\') { escaped = true; continue; }
                if (ch == '\'') { quote = QuoteState::Single; break; }
                if (ch == '"') { quote = QuoteState::Double; break; }
                if (ch == target) return true;
                break;
        }
    }
    return false;
}

bool contains_unquoted_shell_variable_expansion(const std::string& command) {
    QuoteState quote = QuoteState::None;
    bool escaped = false;

    for (size_t i = 0; i < command.size(); ++i) {
        char ch = command[i];
        switch (quote) {
            case QuoteState::Single:
                if (ch == '\'') quote = QuoteState::None;
                continue;
            case QuoteState::Double:
                if (escaped) { escaped = false; continue; }
                if (ch == '\\') { escaped = true; continue; }
                if (ch == '"') { quote = QuoteState::None; continue; }
                break;
            case QuoteState::None:
                if (escaped) { escaped = false; continue; }
                if (ch == '\\') { escaped = true; continue; }
                if (ch == '\'') { quote = QuoteState::Single; continue; }
                if (ch == '"') { quote = QuoteState::Double; continue; }
                break;
        }

        if (ch != '$') continue;
        if (i + 1 >= command.size()) continue;
        char next = command[i + 1];
        if (std::isalnum(static_cast<unsigned char>(next)) ||
            next == '_' || next == '{' || next == '(' || next == '#' ||
            next == '?' || next == '!' || next == '$' || next == '*' ||
            next == '@' || next == '-') {
            return true;
        }
    }
    return false;
}

std::string strip_wrapping_quotes(const std::string& token) {
    if (token.size() >= 2) {
        if ((token.front() == '"' && token.back() == '"') ||
            (token.front() == '\'' && token.back() == '\'')) {
            return token.substr(1, token.size() - 2);
        }
    }
    return token;
}

bool looks_like_path(const std::string& candidate) {
    return !candidate.empty() && (
        candidate[0] == '/' ||
        candidate.substr(0, 2) == "./" ||
        candidate.substr(0, 3) == "../" ||
        candidate[0] == '~' ||
        candidate == "." ||
        candidate == ".." ||
        candidate.find('/') != std::string::npos
    );
}

std::string attached_short_option_value(const std::string& token) {
    if (token.size() < 2 || token[0] != '-') return "";
    std::string body = token.substr(1);
    if (body[0] == '-' || body.size() < 2) return "";
    std::string value = body.substr(1);
    // Trim leading '='
    if (!value.empty() && value[0] == '=') value = value.substr(1);
    // Trim whitespace
    size_t start = value.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    return value.substr(start);
}

std::string redirection_target(const std::string& token) {
    size_t marker = std::string::npos;
    for (size_t i = 0; i < token.size(); ++i) {
        if (token[i] == '<' || token[i] == '>') { marker = i; break; }
    }
    if (marker == std::string::npos) return "";
    std::string rest = token.substr(marker + 1);
    // Strip leading < > & digits
    while (!rest.empty() && (rest[0] == '<' || rest[0] == '>')) rest = rest.substr(1);
    while (!rest.empty() && rest[0] == '&') rest = rest.substr(1);
    while (!rest.empty() && std::isdigit(static_cast<unsigned char>(rest[0]))) rest = rest.substr(1);
    // Trim
    size_t start = rest.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    return rest.substr(start);
}

// ── SecurityPolicy ───────────────────────────────────────────────

SecurityPolicy::SecurityPolicy() {
    autonomy = AutonomyLevel::Supervised;
    workspace_dir = ".";
    workspace_only = true;
    allowed_commands = {
        "git", "npm", "cargo", "ls", "cat", "grep",
        "find", "echo", "pwd", "wc", "head", "tail", "date"
    };
    forbidden_paths = {
        "/etc", "/root", "/home", "/usr", "/bin", "/sbin",
        "/lib", "/opt", "/boot", "/dev", "/proc", "/sys",
        "/var", "/tmp",
        "~/.ssh", "~/.gnupg", "~/.aws", "~/.config"
    };
    max_actions_per_hour = 20;
    max_cost_per_day_cents = 500;
    require_approval_for_medium_risk = true;
    block_high_risk_commands = true;
}

CommandRiskLevel SecurityPolicy::command_risk_level(const std::string& command) const {
    bool saw_medium = false;

    for (const auto& segment : split_unquoted_segments(command)) {
        std::string cmd_part = skip_env_assignments(segment);
        std::istringstream iss(cmd_part);
        std::string base_raw;
        if (!(iss >> base_raw)) continue;

        // Extract base command name (after last '/')
        std::string base;
        auto slash_pos = base_raw.rfind('/');
        if (slash_pos != std::string::npos) {
            base = base_raw.substr(slash_pos + 1);
        } else {
            base = base_raw;
        }
        std::transform(base.begin(), base.end(), base.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        std::vector<std::string> args;
        std::string arg;
        while (iss >> arg) {
            std::transform(arg.begin(), arg.end(), arg.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            args.push_back(arg);
        }

        std::string joined_segment = cmd_part;
        std::transform(joined_segment.begin(), joined_segment.end(), joined_segment.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        // High-risk commands
        static const std::vector<std::string> high_risk = {
            "rm", "mkfs", "dd", "shutdown", "reboot", "halt", "poweroff",
            "sudo", "su", "chown", "chmod", "useradd", "userdel", "usermod",
            "passwd", "mount", "umount", "iptables", "ufw", "firewall-cmd",
            "curl", "wget", "nc", "ncat", "netcat", "scp", "ssh", "ftp", "telnet"
        };
        for (const auto& hr : high_risk) {
            if (base == hr) return CommandRiskLevel::High;
        }

        if (joined_segment.find("rm -rf /") != std::string::npos ||
            joined_segment.find("rm -fr /") != std::string::npos ||
            joined_segment.find(":(){ :|:&};:") != std::string::npos) {
            return CommandRiskLevel::High;
        }

        // Medium-risk classification
        bool medium = false;
        if (base == "git" && !args.empty()) {
            static const std::vector<std::string> git_medium = {
                "commit", "push", "reset", "clean", "rebase", "merge",
                "cherry-pick", "revert", "branch", "checkout", "switch", "tag"
            };
            for (const auto& verb : git_medium) {
                if (args[0] == verb) { medium = true; break; }
            }
        } else if ((base == "npm" || base == "pnpm" || base == "yarn") && !args.empty()) {
            static const std::vector<std::string> npm_medium = {
                "install", "add", "remove", "uninstall", "update", "publish"
            };
            for (const auto& verb : npm_medium) {
                if (args[0] == verb) { medium = true; break; }
            }
        } else if (base == "cargo" && !args.empty()) {
            static const std::vector<std::string> cargo_medium = {
                "add", "remove", "install", "clean", "publish"
            };
            for (const auto& verb : cargo_medium) {
                if (args[0] == verb) { medium = true; break; }
            }
        } else if (base == "touch" || base == "mkdir" || base == "mv" ||
                   base == "cp" || base == "ln") {
            medium = true;
        }

        saw_medium = saw_medium || medium;
    }

    return saw_medium ? CommandRiskLevel::Medium : CommandRiskLevel::Low;
}

SecurityPolicy::CommandValidationResult SecurityPolicy::validate_command_execution(
    const std::string& command, bool approved) const {
    CommandValidationResult result;

    if (!is_command_allowed(command)) {
        result.error = "Command not allowed by security policy: " + command;
        return result;
    }

    auto risk = command_risk_level(command);
    result.risk = risk;

    if (risk == CommandRiskLevel::High) {
        if (block_high_risk_commands) {
            result.error = "Command blocked: high-risk command is disallowed by policy";
            return result;
        }
        if (autonomy == AutonomyLevel::Supervised && !approved) {
            result.error = "Command requires explicit approval (approved=true): high-risk operation";
            return result;
        }
    }

    if (risk == CommandRiskLevel::Medium &&
        autonomy == AutonomyLevel::Supervised &&
        require_approval_for_medium_risk && !approved) {
        result.error = "Command requires explicit approval (approved=true): medium-risk operation";
        return result;
    }

    result.allowed = true;
    return result;
}

bool SecurityPolicy::is_command_allowed(const std::string& command) const {
    if (autonomy == AutonomyLevel::ReadOnly) return false;

    // Block subshell/expansion operators
    if (command.find('`') != std::string::npos ||
        command.find("$(") != std::string::npos ||
        command.find("${") != std::string::npos ||
        contains_unquoted_shell_variable_expansion(command) ||
        command.find("<(") != std::string::npos ||
        command.find(">(") != std::string::npos) {
        return false;
    }

    // Block shell redirections
    if (contains_unquoted_char(command, '>') || contains_unquoted_char(command, '<')) {
        return false;
    }

    // Block tee
    std::istringstream tee_check(command);
    std::string word;
    while (tee_check >> word) {
        if (word == "tee" || (word.size() > 4 && word.substr(word.size() - 4) == "/tee")) {
            return false;
        }
    }

    // Block background command chaining
    if (contains_unquoted_single_ampersand(command)) return false;

    // Split and validate each segment
    auto segments = split_unquoted_segments(command);
    for (const auto& segment : segments) {
        std::string cmd_part = skip_env_assignments(segment);
        std::istringstream iss(cmd_part);
        std::string base_raw;
        if (!(iss >> base_raw)) continue;

        // Extract base command
        std::string base_cmd;
        auto slash_pos = base_raw.rfind('/');
        if (slash_pos != std::string::npos) {
            base_cmd = base_raw.substr(slash_pos + 1);
        } else {
            base_cmd = base_raw;
        }

        if (base_cmd.empty()) continue;

        bool found = false;
        for (const auto& allowed : allowed_commands) {
            if (allowed == base_cmd) { found = true; break; }
        }
        if (!found) return false;

        // Validate arguments
        std::vector<std::string> args;
        std::string arg;
        while (iss >> arg) {
            std::transform(arg.begin(), arg.end(), arg.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            args.push_back(arg);
        }
        if (!is_args_safe(base_cmd, args)) return false;
    }

    // At least one command must be present
    bool has_cmd = false;
    for (const auto& s : segments) {
        std::string trimmed = skip_env_assignments(s);
        std::istringstream iss(trimmed);
        std::string w;
        if (iss >> w && !w.empty()) { has_cmd = true; break; }
    }

    return has_cmd;
}

bool SecurityPolicy::is_args_safe(const std::string& base, const std::vector<std::string>& args) const {
    std::string base_lower = base;
    std::transform(base_lower.begin(), base_lower.end(), base_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (base_lower == "find") {
        for (const auto& arg : args) {
            if (arg == "-exec" || arg == "-ok") return false;
        }
    } else if (base_lower == "git") {
        for (const auto& arg : args) {
            if (arg == "config" || arg.substr(0, 7) == "config." ||
                arg == "alias" || arg.substr(0, 6) == "alias." ||
                arg == "-c") {
                return false;
            }
        }
    }
    return true;
}

std::string SecurityPolicy::forbidden_path_argument(const std::string& command) const {
    auto forbidden_candidate = [this](const std::string& raw) -> std::string {
        std::string candidate = strip_wrapping_quotes(raw);
        // Trim
        size_t start = candidate.find_first_not_of(" \t");
        size_t end = candidate.find_last_not_of(" \t");
        if (start == std::string::npos) return "";
        candidate = candidate.substr(start, end - start + 1);

        if (candidate.empty() || candidate.find("://") != std::string::npos) return "";
        if (looks_like_path(candidate) && !is_path_allowed(candidate)) {
            return candidate;
        }
        return "";
    };

    for (const auto& segment : split_unquoted_segments(command)) {
        std::string cmd_part = skip_env_assignments(segment);
        std::istringstream iss(cmd_part);
        std::string executable;
        if (!(iss >> executable)) continue;

        // Check executable for inline redirection
        auto target = redirection_target(strip_wrapping_quotes(executable));
        if (!target.empty()) {
            auto blocked = forbidden_candidate(target);
            if (!blocked.empty()) return blocked;
        }

        std::string token;
        while (iss >> token) {
            std::string candidate = strip_wrapping_quotes(token);
            size_t start = candidate.find_first_not_of(" \t");
            size_t end = candidate.find_last_not_of(" \t");
            if (start == std::string::npos) continue;
            candidate = candidate.substr(start, end - start + 1);

            if (candidate.empty() || candidate.find("://") != std::string::npos) continue;

            auto redir = redirection_target(candidate);
            if (!redir.empty()) {
                auto blocked = forbidden_candidate(redir);
                if (!blocked.empty()) return blocked;
            }

            // Handle option assignment: --file=/etc/passwd
            if (candidate[0] == '-') {
                auto eq_pos = candidate.find('=');
                if (eq_pos != std::string::npos) {
                    auto blocked = forbidden_candidate(candidate.substr(eq_pos + 1));
                    if (!blocked.empty()) return blocked;
                }
                auto attached = attached_short_option_value(candidate);
                if (!attached.empty()) {
                    auto blocked = forbidden_candidate(attached);
                    if (!blocked.empty()) return blocked;
                }
                continue;
            }

            auto blocked = forbidden_candidate(candidate);
            if (!blocked.empty()) return blocked;
        }
    }

    return "";
}

bool SecurityPolicy::is_path_allowed(const std::string& path) const {
    // Block null bytes
    if (path.find('\0') != std::string::npos) return false;

    // Block path traversal: check for ".." components
    auto fs_path = std::filesystem::path(path);
    for (const auto& component : fs_path) {
        if (component == "..") return false;
    }

    // Block URL-encoded traversal
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower.find("..%2f") != std::string::npos || lower.find("%2f..") != std::string::npos) {
        return false;
    }

    // Reject ~user forms
    if (!path.empty() && path[0] == '~' && path != "~" &&
        (path.size() < 2 || path[1] != '/')) {
        return false;
    }

    auto expanded_path = expand_user_path(path);

    // Block absolute paths when workspace_only
    if (workspace_only && expanded_path.is_absolute()) return false;

    // Block forbidden paths
    for (const auto& forbidden : forbidden_paths) {
        auto forbidden_path = expand_user_path(forbidden);
        auto expanded_str = expanded_path.string();
        auto forbidden_str = forbidden_path.string();
        if (expanded_str.substr(0, forbidden_str.size()) == forbidden_str) {
            return false;
        }
    }

    return true;
}

bool SecurityPolicy::is_resolved_path_allowed(const std::filesystem::path& resolved) const {
    std::error_code ec;
    auto workspace_root = std::filesystem::canonical(workspace_dir, ec);
    if (ec) workspace_root = workspace_dir;

    auto resolved_str = resolved.string();
    auto workspace_str = workspace_root.string();
    if (resolved_str.substr(0, workspace_str.size()) == workspace_str) {
        return true;
    }

    for (const auto& root : allowed_roots) {
        auto canonical = std::filesystem::canonical(root, ec);
        if (ec) canonical = root;
        auto root_str = canonical.string();
        if (resolved_str.substr(0, root_str.size()) == root_str) {
            return true;
        }
    }

    return false;
}

std::string SecurityPolicy::resolved_path_violation_message(const std::filesystem::path& resolved) const {
    std::string guidance;
    if (allowed_roots.empty()) {
        guidance = "Add the directory to [autonomy].allowed_roots (for example: allowed_roots = "
                   "[\"/absolute/path\"]), or move the file into the workspace.";
    } else {
        guidance = "Add a matching parent directory to [autonomy].allowed_roots, "
                   "or move the file into the workspace.";
    }
    return "Resolved path escapes workspace allowlist: " + resolved.string() + ". " + guidance;
}

bool SecurityPolicy::can_act() const {
    return autonomy != AutonomyLevel::ReadOnly;
}

SecurityPolicy::ToolOperationResult SecurityPolicy::enforce_tool_operation(
    ToolOperation operation, const std::string& operation_name) const {
    ToolOperationResult result;

    if (operation == ToolOperation::Read) {
        result.allowed = true;
        return result;
    }

    // Act operation
    if (!can_act()) {
        result.error = "Security policy: read-only mode, cannot perform '" + operation_name + "'";
        return result;
    }

    if (!record_action()) {
        result.error = "Rate limit exceeded: action budget exhausted";
        return result;
    }

    result.allowed = true;
    return result;
}

bool SecurityPolicy::record_action() const {
    size_t count = const_cast<ActionTracker&>(tracker).record();
    return count <= static_cast<size_t>(max_actions_per_hour);
}

bool SecurityPolicy::is_rate_limited() const {
    return const_cast<ActionTracker&>(tracker).count() >= static_cast<size_t>(max_actions_per_hour);
}

} // namespace security
} // namespace zeroclaw
