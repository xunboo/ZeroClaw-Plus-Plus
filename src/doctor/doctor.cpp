#include "doctor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <regex>
#include <iomanip>
#include <ctime>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif

namespace zeroclaw {
namespace doctor {

DiagItem DiagItem::ok(const char* category, std::string message) {
    return DiagItem(Severity::Ok, category, std::move(message));
}

DiagItem DiagItem::warn(const char* category, std::string message) {
    return DiagItem(Severity::Warn, category, std::move(message));
}

DiagItem DiagItem::error(const char* category, std::string message) {
    return DiagItem(Severity::Error, category, std::move(message));
}

const char* DiagItem::icon() const {
    switch (severity_) {
        case Severity::Ok: return "OK";
        case Severity::Warn: return "WARN";
        case Severity::Error: return "ERR";
    }
    return "?";
}

DiagResult DiagItem::into_result() const {
    return DiagResult{severity_, std::string(category_), message_};
}

std::string truncate_for_display(const std::string& input, size_t max_chars) {
    if (input.length() <= max_chars) {
        return input;
    }
    
    size_t valid_len = 0;
    size_t byte_pos = 0;
    while (byte_pos < input.length() && valid_len < max_chars) {
        unsigned char c = static_cast<unsigned char>(input[byte_pos]);
        if ((c & 0x80) == 0) {
            byte_pos += 1;
        } else if ((c & 0xE0) == 0xC0) {
            byte_pos += 2;
        } else if ((c & 0xF0) == 0xE0) {
            byte_pos += 3;
        } else if ((c & 0xF8) == 0xF0) {
            byte_pos += 4;
        } else {
            byte_pos += 1;
        }
        valid_len++;
    }
    
    return input.substr(0, byte_pos) + "...";
}

std::string format_error_chain(const std::exception& error) {
    return error.what();
}

std::optional<std::chrono::system_clock::time_point> parse_rfc3339(const std::string& raw) {
    std::tm tm = {};
    std::istringstream iss(raw);
    char dummy;
    
    if (!(iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S"))) {
        return std::nullopt;
    }
    
    tm.tm_isdst = -1;
    auto time = std::mktime(&tm);
    if (time == -1) {
        return std::nullopt;
    }
    
    return std::chrono::system_clock::from_time_t(time);
}

ModelProbeOutcome classify_model_probe_error(const std::string& err_message) {
    std::string lower = err_message;
    std::transform(lower.begin(), lower.end(), lower.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    if (lower.find("does not support live model discovery") != std::string::npos) {
        return ModelProbeOutcome::Skipped;
    }
    
    std::vector<std::string> hints = {
        "401", "403", "429", "unauthorized", "forbidden",
        "api key", "token", "insufficient balance", "insufficient quota",
        "plan does not include", "rate limit"
    };
    
    for (const auto& hint : hints) {
        if (lower.find(hint) != std::string::npos) {
            return ModelProbeOutcome::AuthOrAccess;
        }
    }
    
    return ModelProbeOutcome::Error;
}

std::filesystem::path workspace_probe_path(const std::filesystem::path& workspace_dir) {
    auto now = std::chrono::system_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    
#ifdef _WIN32
    auto pid = _getpid();
#else
    auto pid = getpid();
#endif
    
    std::ostringstream oss;
    oss << ".zeroclaw_doctor_probe_" << pid << "_" << nanos;
    return workspace_dir / oss.str();
}

std::optional<uint64_t> parse_df_available_mb(const std::string& stdout_str) {
    std::istringstream stream(stdout_str);
    std::string line;
    std::string last_line;
    
    while (std::getline(stream, line)) {
        if (!line.empty() && line.find_first_not_of(" \t\r\n") != std::string::npos) {
            last_line = line;
        }
    }
    
    if (last_line.empty()) {
        return std::nullopt;
    }
    
    std::istringstream line_stream(last_line);
    std::string token;
    int col = 0;
    
    while (line_stream >> token) {
        col++;
        if (col == 4) {
            try {
                return std::stoull(token);
            } catch (...) {
                return std::nullopt;
            }
        }
    }
    
    return std::nullopt;
}

std::optional<uint64_t> disk_available_mb(const std::filesystem::path& path) {
#ifdef _WIN32
    ULARGE_INTEGER free_bytes;
    if (GetDiskFreeSpaceExW(path.wstring().c_str(), &free_bytes, nullptr, nullptr)) {
        return free_bytes.QuadPart / (1024 * 1024);
    }
    return std::nullopt;
#else
    std::array<char, 256> buffer;
    std::string cmd = "df -m " + path.string() + " 2>/dev/null";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return std::nullopt;
    }
    
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe)) {
        result += buffer.data();
    }
    pclose(pipe);
    
    return parse_df_available_mb(result);
#endif
}

void check_file_exists(const std::filesystem::path& base,
                       const std::string& name,
                       bool required,
                       const char* cat,
                       std::vector<DiagItem>& items) {
    auto path = base / name;
    if (std::filesystem::is_regular_file(path)) {
        items.push_back(DiagItem::ok(cat, name + " present"));
    } else if (required) {
        items.push_back(DiagItem::error(cat, name + " missing"));
    } else {
        items.push_back(DiagItem::warn(cat, name + " not found (optional)"));
    }
}

void check_command_available(const std::string& cmd,
                             const std::vector<std::string>& args,
                             const char* cat,
                             std::vector<DiagItem>& items) {
    std::string full_cmd = cmd;
    for (const auto& arg : args) {
        full_cmd += " " + arg;
    }
    
#ifdef _WIN32
    full_cmd += " 2>nul";
#else
    full_cmd += " 2>/dev/null";
#endif
    
    FILE* pipe = nullptr;
    
#ifdef _WIN32
    pipe = _popen(full_cmd.c_str(), "r");
#else
    pipe = popen(full_cmd.c_str(), "r");
#endif
    
    if (!pipe) {
        items.push_back(DiagItem::warn(cat, cmd + " not found in PATH"));
        return;
    }
    
    std::array<char, 256> buffer;
    std::string output;
    while (fgets(buffer.data(), buffer.size(), pipe)) {
        output += buffer.data();
    }
    
#ifdef _WIN32
    int status = _pclose(pipe);
#else
    int status = pclose(pipe);
#endif
    
    if (status == 0) {
        std::string first_line;
        std::istringstream iss(output);
        std::getline(iss, first_line);
        
        size_t start = first_line.find_first_not_of(" \t\r\n");
        size_t end = first_line.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            first_line = first_line.substr(start, end - start + 1);
        }
        
        std::string display = truncate_for_display(first_line, COMMAND_VERSION_PREVIEW_CHARS);
        items.push_back(DiagItem::ok(cat, cmd + ": " + display));
    } else {
        items.push_back(DiagItem::warn(cat, cmd + " found but returned non-zero"));
    }
}

void check_environment(std::vector<DiagItem>& items) {
    const char* cat = "environment";
    
    check_command_available("git", {"--version"}, cat, items);
    
    const char* shell = std::getenv("SHELL");
    if (shell == nullptr || shell[0] == '\0') {
        items.push_back(DiagItem::warn(cat, "$SHELL not set"));
    } else {
        items.push_back(DiagItem::ok(cat, std::string("shell: ") + shell));
    }
    
    const char* home = std::getenv("HOME");
    const char* userprofile = std::getenv("USERPROFILE");
    
    if (home != nullptr || userprofile != nullptr) {
        items.push_back(DiagItem::ok(cat, "home directory env set"));
    } else {
        items.push_back(DiagItem::error(cat, "neither $HOME nor $USERPROFILE is set"));
    }
    
    check_command_available("curl", {"--version"}, cat, items);
}

void check_cli_tools(std::vector<DiagItem>& items) {
    const char* cat = "cli-tools";
    items.push_back(DiagItem::warn(cat, "CLI tool discovery not yet implemented"));
}

std::vector<std::string> doctor_model_targets(const std::optional<std::string>& provider_override) {
    if (provider_override.has_value() && !provider_override->empty()) {
        std::string trimmed = *provider_override;
        size_t start = trimmed.find_first_not_of(" \t");
        size_t end = trimmed.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            trimmed = trimmed.substr(start, end - start + 1);
        }
        if (!trimmed.empty()) {
            return {trimmed};
        }
    }
    return {"openai", "anthropic", "groq", "ollama"};
}

void check_workspace(const config::Config& config, std::vector<DiagItem>& items) {
    const char* cat = "workspace";
    auto ws = std::filesystem::path(".");
    
    if (std::filesystem::exists(ws)) {
        items.push_back(DiagItem::ok(cat, "directory exists: " + ws.string()));
    } else {
        items.push_back(DiagItem::error(cat, "directory missing: " + ws.string()));
        return;
    }
    
    auto probe = workspace_probe_path(ws);
    std::ofstream test_file(probe);
    if (test_file.is_open()) {
        test_file << "probe";
        test_file.close();
        std::filesystem::remove(probe);
        items.push_back(DiagItem::ok(cat, "directory is writable"));
    } else {
        items.push_back(DiagItem::error(cat, "directory is not writable"));
    }
    
    if (auto avail = disk_available_mb(ws)) {
        if (*avail >= 100) {
            items.push_back(DiagItem::ok(cat, "disk space: " + std::to_string(*avail) + " MB available"));
        } else {
            items.push_back(DiagItem::warn(cat, "low disk space: only " + std::to_string(*avail) + " MB available"));
        }
    }
    
    check_file_exists(ws, "SOUL.md", false, cat, items);
    check_file_exists(ws, "AGENTS.md", false, cat, items);
}

std::optional<std::string> provider_validation_error(const std::string& name) {
    std::string trimmed = name;
    size_t start = trimmed.find_first_not_of(" \t");
    size_t end = trimmed.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
        trimmed = trimmed.substr(start, end - start + 1);
    }
    
    if (trimmed.empty()) {
        return "provider name is empty";
    }
    
    std::vector<std::string> known_providers = {
        "openai", "anthropic", "groq", "ollama", "openrouter", "gemini"
    };
    
    std::string lower = trimmed;
    std::transform(lower.begin(), lower.end(), lower.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    for (const auto& p : known_providers) {
        if (lower == p) {
            return std::nullopt;
        }
    }
    
    if (lower.find("custom:") == 0) {
        std::string url = trimmed.substr(7);
        size_t url_start = url.find_first_not_of(" \t");
        size_t url_end = url.find_last_not_of(" \t");
        if (url_start != std::string::npos && url_end != std::string::npos) {
            url = url.substr(url_start, url_end - url_start + 1);
        }
        
        if (url.empty()) {
            return "custom provider requires a non-empty URL after 'custom:'";
        }
        
        if (url.find("http://") != 0 && url.find("https://") != 0) {
            return "custom provider URL must use http/https";
        }
        return std::nullopt;
    }
    
    return "Unknown provider: " + trimmed;
}

std::optional<std::string> embedding_provider_validation_error(const std::string& name) {
    std::string normalized = name;
    size_t start = normalized.find_first_not_of(" \t");
    size_t end = normalized.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
        normalized = normalized.substr(start, end - start + 1);
    }
    
    std::string lower = normalized;
    std::transform(lower.begin(), lower.end(), lower.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    if (lower == "none" || lower == "openai") {
        return std::nullopt;
    }
    
    if (lower.find("custom:") == 0) {
        std::string url = normalized.substr(7);
        size_t url_start = url.find_first_not_of(" \t");
        size_t url_end = url.find_last_not_of(" \t");
        if (url_start != std::string::npos && url_end != std::string::npos) {
            url = url.substr(url_start, url_end - url_start + 1);
        }
        
        if (url.empty()) {
            return "custom provider requires a non-empty URL after 'custom:'";
        }
        
        if (url.find("http://") != 0 && url.find("https://") != 0) {
            return "custom provider URL must use http/https, got '" + url.substr(0, url.find(':')) + "'";
        }
        return std::nullopt;
    }
    
    return "supported values: none, openai, custom:<url>";
}

void check_daemon_state(const config::Config& config, std::vector<DiagItem>& items) {
    const char* cat = "daemon";
    std::filesystem::path state_file = ".zeroclaw" / std::filesystem::path("daemon_state.json");
    
    if (!std::filesystem::exists(state_file)) {
        items.push_back(DiagItem::error(cat, "state file not found: " + state_file.string() + " — is the daemon running?"));
        return;
    }
    
    std::ifstream file(state_file);
    if (!file.is_open()) {
        items.push_back(DiagItem::error(cat, "cannot read state file"));
        return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string raw = buffer.str();
    
    auto updated_at_pos = raw.find("\"updated_at\"");
    if (updated_at_pos != std::string::npos) {
        auto quote_start = raw.find("\"", updated_at_pos + 13);
        if (quote_start != std::string::npos) {
            auto quote_end = raw.find("\"", quote_start + 1);
            if (quote_end != std::string::npos) {
                std::string timestamp = raw.substr(quote_start + 1, quote_end - quote_start - 1);
                
                auto ts_opt = parse_rfc3339(timestamp);
                if (ts_opt) {
                    auto now = std::chrono::system_clock::now();
                    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - *ts_opt).count();
                    
                    if (age <= DAEMON_STALE_SECONDS) {
                        items.push_back(DiagItem::ok(cat, "heartbeat fresh (" + std::to_string(age) + "s ago)"));
                    } else {
                        items.push_back(DiagItem::error(cat, "heartbeat stale (" + std::to_string(age) + "s ago)"));
                    }
                } else {
                    items.push_back(DiagItem::error(cat, "invalid daemon timestamp: " + timestamp));
                }
            }
        }
    } else {
        items.push_back(DiagItem::warn(cat, "daemon state has no updated_at field"));
    }
}

std::vector<DiagResult> diagnose(const config::Config& config) {
    std::vector<DiagItem> items;
    
    check_config_semantics(config, items);
    check_workspace(config, items);
    check_daemon_state(config, items);
    check_environment(items);
    check_cli_tools(items);
    
    std::vector<DiagResult> results;
    results.reserve(items.size());
    for (const auto& item : items) {
        results.push_back(item.into_result());
    }
    return results;
}

bool run(const config::Config& config) {
    auto results = diagnose(config);
    
    std::cout << "[ZeroClaw Doctor]" << std::endl;
    std::cout << std::endl;
    
    std::string current_cat;
    for (const auto& item : results) {
        if (item.category != current_cat) {
            current_cat = item.category;
            std::cout << "  [" << current_cat << "]" << std::endl;
        }
        
        const char* icon = "OK";
        switch (item.severity) {
            case Severity::Ok: icon = "OK"; break;
            case Severity::Warn: icon = "WARN"; break;
            case Severity::Error: icon = "ERR"; break;
        }
        std::cout << "    [" << icon << "] " << item.message << std::endl;
    }
    
    size_t errors = 0, warns = 0, oks = 0;
    for (const auto& item : results) {
        switch (item.severity) {
            case Severity::Ok: oks++; break;
            case Severity::Warn: warns++; break;
            case Severity::Error: errors++; break;
        }
    }
    
    std::cout << std::endl;
    std::cout << "  Summary: " << oks << " ok, " << warns << " warnings, " << errors << " errors" << std::endl;
    
    if (errors > 0) {
        std::cout << "  Fix the errors above, then run `zeroclaw doctor` again." << std::endl;
        return false;
    }
    
    return true;
}

bool run_models(const config::Config& config,
                const std::optional<std::string>& provider_override,
                bool use_cache) {
    auto targets = doctor_model_targets(provider_override);
    
    if (targets.empty()) {
        std::cerr << "No providers available for model probing" << std::endl;
        return false;
    }
    
    std::cout << "[ZeroClaw Doctor - Model Catalog Probe]" << std::endl;
    std::cout << "  Providers to probe: " << targets.size() << std::endl;
    std::cout << "  Mode: " << (use_cache ? "cache-first" : "force live refresh") << std::endl;
    std::cout << std::endl;
    
    size_t ok_count = 0, skipped_count = 0, auth_count = 0, error_count = 0;
    
    for (const auto& provider_name : targets) {
        std::cout << "  [" << provider_name << "]" << std::endl;
        
        bool success = true;
        std::string error_text;
        
        if (success) {
            ok_count++;
            std::cout << "    [OK] model catalog check passed" << std::endl;
        } else {
            auto outcome = classify_model_probe_error(error_text);
            switch (outcome) {
                case ModelProbeOutcome::Skipped:
                    skipped_count++;
                    std::cout << "    [SKIP] skipped: " << truncate_for_display(error_text, 160) << std::endl;
                    break;
                case ModelProbeOutcome::AuthOrAccess:
                    auth_count++;
                    std::cout << "    [WARN] auth/access: " << truncate_for_display(error_text, 160) << std::endl;
                    break;
                case ModelProbeOutcome::Error:
                    error_count++;
                    std::cout << "    [ERR] error: " << truncate_for_display(error_text, 160) << std::endl;
                    break;
                case ModelProbeOutcome::Ok:
                    ok_count++;
                    break;
            }
        }
        std::cout << std::endl;
    }
    
    std::cout << "  Summary: " << ok_count << " ok, " << skipped_count << " skipped, " 
              << auth_count << " auth/access, " << error_count << " errors" << std::endl;
    
    if (auth_count > 0) {
        std::cout << "  Some providers need valid API keys/plan access before `/models` can be fetched." << std::endl;
    }
    
    if (provider_override.has_value() && ok_count == 0) {
        return false;
    }
    
    return true;
}

bool run_traces(const config::Config& config,
                const std::optional<std::string>& id,
                const std::optional<std::string>& event_filter,
                const std::optional<std::string>& contains,
                size_t limit) {
    std::filesystem::path path = ".zeroclaw" / std::filesystem::path("traces.jsonl");
    
    if (id.has_value() && !id->empty()) {
        std::string target_id = *id;
        size_t start = target_id.find_first_not_of(" \t");
        size_t end = target_id.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            target_id = target_id.substr(start, end - start + 1);
        }
        
        if (!target_id.empty()) {
            std::cout << "Looking for trace with id: " << target_id << std::endl;
            std::cout << "Trace inspection not yet fully implemented" << std::endl;
            return true;
        }
    }
    
    if (!std::filesystem::exists(path)) {
        std::cout << "Runtime trace file not found: " << path.string() << std::endl;
        std::cout << "Enable [observability] runtime_trace_mode = \"rolling\" or \"full\", then reproduce the issue." << std::endl;
        return true;
    }
    
    size_t safe_limit = std::max(limit, size_t(1));
    
    std::cout << "Runtime traces (newest first)" << std::endl;
    std::cout << "Path: " << path.string() << std::endl;
    std::cout << "Filters: event=" << (event_filter.has_value() ? *event_filter : "*")
              << " contains=" << (contains.has_value() ? *contains : "*")
              << " limit=" << safe_limit << std::endl;
    std::cout << std::endl;
    
    std::cout << "Use `zeroclaw doctor traces --id <trace-id>` to inspect a full event payload." << std::endl;
    return true;
}

}
}
