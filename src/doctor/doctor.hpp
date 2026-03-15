#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <filesystem>
#include <map>
#include <functional>
#include <memory>

namespace zeroclaw {
namespace config {
    struct Config;
    struct ModelRouteConfig;
    struct EmbeddingRouteConfig;
    struct DelegateAgentConfig;
}

namespace doctor {

inline constexpr int64_t DAEMON_STALE_SECONDS = 30;
inline constexpr int64_t SCHEDULER_STALE_SECONDS = 120;
inline constexpr int64_t CHANNEL_STALE_SECONDS = 300;
inline constexpr size_t COMMAND_VERSION_PREVIEW_CHARS = 60;

enum class Severity {
    Ok,
    Warn,
    Error
};

struct DiagResult {
    Severity severity;
    std::string category;
    std::string message;
};

class DiagItem {
public:
    static DiagItem ok(const char* category, std::string message);
    static DiagItem warn(const char* category, std::string message);
    static DiagItem error(const char* category, std::string message);

    Severity get_severity() const { return severity_; }
    const char* get_category() const { return category_; }
    const std::string& get_message() const { return message_; }
    const char* icon() const;
    DiagResult into_result() const;

private:
    DiagItem(Severity severity, const char* category, std::string message)
        : severity_(severity), category_(category), message_(std::move(message)) {}

    Severity severity_;
    const char* category_;
    std::string message_;
};

std::vector<DiagResult> diagnose(const config::Config& config);

bool run(const config::Config& config);

enum class ModelProbeOutcome {
    Ok,
    Skipped,
    AuthOrAccess,
    Error
};

ModelProbeOutcome classify_model_probe_error(const std::string& err_message);

std::vector<std::string> doctor_model_targets(const std::optional<std::string>& provider_override);

bool run_models(const config::Config& config,
                const std::optional<std::string>& provider_override,
                bool use_cache);

bool run_traces(const config::Config& config,
                const std::optional<std::string>& id,
                const std::optional<std::string>& event_filter,
                const std::optional<std::string>& contains,
                size_t limit);

void check_config_semantics(const config::Config& config, std::vector<DiagItem>& items);

std::optional<std::string> provider_validation_error(const std::string& name);

std::optional<std::string> embedding_provider_validation_error(const std::string& name);

void check_workspace(const config::Config& config, std::vector<DiagItem>& items);

void check_file_exists(const std::filesystem::path& base,
                       const std::string& name,
                       bool required,
                       const char* cat,
                       std::vector<DiagItem>& items);

std::optional<uint64_t> disk_available_mb(const std::filesystem::path& path);

std::optional<uint64_t> parse_df_available_mb(const std::string& stdout_str);

std::filesystem::path workspace_probe_path(const std::filesystem::path& workspace_dir);

void check_daemon_state(const config::Config& config, std::vector<DiagItem>& items);

void check_environment(std::vector<DiagItem>& items);

void check_cli_tools(std::vector<DiagItem>& items);

void check_command_available(const std::string& cmd,
                             const std::vector<std::string>& args,
                             const char* cat,
                             std::vector<DiagItem>& items);

std::string format_error_chain(const std::exception& error);

std::string truncate_for_display(const std::string& input, size_t max_chars);

std::optional<std::chrono::system_clock::time_point> parse_rfc3339(const std::string& raw);

}
}
