#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <filesystem>
#include <cstdint>
#include <memory>

#include "traits.hpp"

namespace zeroclaw {
namespace config {

enum class StreamMode {
    Off,
    Partial
};

enum class ProxyScope {
    Environment,
    Zeroclaw,
    Services
};

enum class HardwareTransport {
    None,
    Native,
    Serial,
    Probe
};

enum class SkillsPromptInjectionMode {
    Full,
    Compact
};

enum class SandboxBackend {
    Auto,
    Landlock,
    Firejail,
    Bubblewrap,
    Docker,
    None
};

enum class OtpMethod {
    Totp,
    Pairing,
    CliPrompt
};

enum class LarkReceiveMode {
    Websocket,
    Webhook
};

struct ModelPricing {
    double input = 0.0;
    double output = 0.0;
};

struct DelegateAgentConfig {
    std::string provider;
    std::string model;
    std::optional<std::string> system_prompt;
    std::optional<std::string> api_key;
    std::optional<double> temperature;
    uint32_t max_depth = 3;
    bool agentic = false;
    std::vector<std::string> allowed_tools;
    size_t max_iterations = 10;
};

struct HardwareConfig {
    bool enabled = false;
    HardwareTransport transport = HardwareTransport::None;
    std::optional<std::string> serial_port;
    uint32_t baud_rate = 115200;
    std::optional<std::string> probe_target;
    bool workspace_datasheets = false;

    HardwareTransport transport_mode() const;
};

struct TranscriptionConfig {
    bool enabled = false;
    std::string api_url = "https://api.groq.com/openai/v1/audio/transcriptions";
    std::string model = "whisper-large-v3-turbo";
    std::optional<std::string> language;
    uint64_t max_duration_secs = 120;
};

struct AgentConfig {
    bool compact_context = false;
    size_t max_tool_iterations = 10;
    size_t max_history_messages = 50;
    bool parallel_tools = false;
    std::string tool_dispatcher = "auto";
};

struct SkillsConfig {
    bool open_skills_enabled = false;
    std::optional<std::string> open_skills_dir;
    SkillsPromptInjectionMode prompt_injection_mode = SkillsPromptInjectionMode::Full;
};

struct MultimodalConfig {
    size_t max_images = 4;
    size_t max_image_size_mb = 5;
    bool allow_remote_fetch = false;

    std::pair<size_t, size_t> effective_limits() const;
};

struct IdentityConfig {
    std::string format = "openclaw";
    std::optional<std::string> aieos_path;
    std::optional<std::string> aieos_inline;
};

struct CostConfig {
    bool enabled = false;
    double daily_limit_usd = 10.0;
    double monthly_limit_usd = 100.0;
    uint8_t warn_at_percent = 80;
    bool allow_override = false;
    std::unordered_map<std::string, ModelPricing> prices;
};

struct PeripheralBoardConfig {
    std::string board;
    std::string transport = "serial";
    std::optional<std::string> path;
    uint32_t baud = 115200;
};

struct PeripheralsConfig {
    bool enabled = false;
    std::vector<PeripheralBoardConfig> boards;
    std::optional<std::string> datasheet_dir;
};

struct GatewayConfig {
    uint16_t port = 42617;
    std::string host = "127.0.0.1";
    bool require_pairing = true;
    bool allow_public_bind = false;
    std::vector<std::string> paired_tokens;
    uint32_t pair_rate_limit_per_minute = 10;
    uint32_t webhook_rate_limit_per_minute = 60;
    bool trust_forwarded_headers = false;
    size_t rate_limit_max_keys = 10000;
    uint64_t idempotency_ttl_secs = 300;
    size_t idempotency_max_keys = 10000;
};

struct ComposioConfig {
    bool enabled = false;
    std::optional<std::string> api_key;
    std::string entity_id = "default";
};

struct SecretsConfig {
    bool encrypt = true;
};

struct BrowserComputerUseConfig {
    std::string endpoint = "http://127.0.0.1:8787/v1/actions";
    std::optional<std::string> api_key;
    uint64_t timeout_ms = 15000;
    bool allow_remote_endpoint = false;
    std::vector<std::string> window_allowlist;
    std::optional<int64_t> max_coordinate_x;
    std::optional<int64_t> max_coordinate_y;
};

struct BrowserConfig {
    bool enabled = false;
    std::vector<std::string> allowed_domains;
    std::optional<std::string> session_name;
    std::string backend = "agent_browser";
    bool native_headless = true;
    std::string native_webdriver_url = "http://127.0.0.1:9515";
    std::optional<std::string> native_chrome_path;
    BrowserComputerUseConfig computer_use;
};

struct HttpRequestConfig {
    bool enabled = false;
    std::vector<std::string> allowed_domains;
    size_t max_response_size = 1000000;
    uint64_t timeout_secs = 30;
};

struct WebSearchConfig {
    bool enabled = false;
    std::string provider = "duckduckgo";
    std::optional<std::string> brave_api_key;
    size_t max_results = 5;
    uint64_t timeout_secs = 15;
};

struct ProxyConfig {
    bool enabled = false;
    std::optional<std::string> http_proxy;
    std::optional<std::string> https_proxy;
    std::optional<std::string> all_proxy;
    std::vector<std::string> no_proxy;
    ProxyScope scope = ProxyScope::Zeroclaw;
    std::vector<std::string> services;

    static const std::vector<std::string>& supported_service_keys();
    static const std::vector<std::string>& supported_service_selectors();
    bool has_any_proxy_url() const;
    std::vector<std::string> normalized_services() const;
    std::vector<std::string> normalized_no_proxy() const;
    bool should_apply_to_service(const std::string& service_key) const;
};

struct StorageProviderConfig {
    std::string provider;
    std::optional<std::string> db_url;
    std::string schema = "public";
    std::string table = "memories";
    std::optional<uint64_t> connect_timeout_secs;
};

struct StorageProviderSection {
    StorageProviderConfig config;
};

struct StorageConfig {
    StorageProviderSection provider;
};

struct MemoryConfig {
    std::string backend = "sqlite";
    bool auto_save = true;
    bool hygiene_enabled = true;
    uint32_t archive_after_days = 7;
    uint32_t purge_after_days = 30;
    uint32_t conversation_retention_days = 30;
    std::string embedding_provider = "none";
    std::string embedding_model = "text-embedding-3-small";
    size_t embedding_dimensions = 1536;
    double vector_weight = 0.7;
    double keyword_weight = 0.3;
    double min_relevance_score = 0.4;
    size_t embedding_cache_size = 10000;
    size_t chunk_max_tokens = 512;
    bool response_cache_enabled = false;
    uint32_t response_cache_ttl_minutes = 60;
    size_t response_cache_max_entries = 5000;
    bool snapshot_enabled = false;
    bool snapshot_on_hygiene = false;
    bool auto_hydrate = true;
    std::optional<uint64_t> sqlite_open_timeout_secs;
};

struct ObservabilityConfig {
    std::string backend = "none";
    std::optional<std::string> otel_endpoint;
    std::optional<std::string> otel_service_name;
    std::string runtime_trace_mode = "none";
    std::string runtime_trace_path = "state/runtime-trace.jsonl";
    size_t runtime_trace_max_entries = 200;
};

struct BuiltinHooksConfig {
    bool command_logger = false;
};

struct HooksConfig {
    bool enabled = true;
    BuiltinHooksConfig builtin;
};

struct AutonomyConfig {
    std::string level = "supervised";
    bool workspace_only = true;
    std::vector<std::string> allowed_commands;
    std::vector<std::string> forbidden_paths;
    uint32_t max_actions_per_hour = 20;
    uint32_t max_cost_per_day_cents = 500;
    bool require_approval_for_medium_risk = true;
    bool block_high_risk_commands = true;
    std::vector<std::string> shell_env_passthrough;
    std::vector<std::string> auto_approve;
    std::vector<std::string> always_ask;
    std::vector<std::string> allowed_roots;
    std::vector<std::string> non_cli_excluded_tools;
};

struct DockerRuntimeConfig {
    std::string image = "alpine:3.20";
    std::string network = "none";
    std::optional<uint64_t> memory_limit_mb = 512;
    std::optional<double> cpu_limit = 1.0;
    bool read_only_rootfs = true;
    bool mount_workspace = true;
    std::vector<std::string> allowed_workspace_roots;
};

struct RuntimeConfig {
    std::string kind = "native";
    DockerRuntimeConfig docker;
    std::optional<bool> reasoning_enabled;
};

struct ReliabilityConfig {
    uint32_t provider_retries = 2;
    uint64_t provider_backoff_ms = 500;
    std::vector<std::string> fallback_providers;
    std::vector<std::string> api_keys;
    std::unordered_map<std::string, std::vector<std::string>> model_fallbacks;
    uint64_t channel_initial_backoff_secs = 2;
    uint64_t channel_max_backoff_secs = 60;
    uint64_t scheduler_poll_secs = 15;
    uint32_t scheduler_retries = 2;
};

struct SchedulerConfig {
    bool enabled = true;
    size_t max_tasks = 64;
    size_t max_concurrent = 4;
};

struct ModelRouteConfig {
    std::string hint;
    std::string provider;
    std::string model;
    std::optional<std::string> api_key;
};

struct EmbeddingRouteConfig {
    std::string hint;
    std::string provider;
    std::string model;
    std::optional<size_t> dimensions;
    std::optional<std::string> api_key;
};

struct ClassificationRule {
    std::string hint;
    std::vector<std::string> keywords;
    std::vector<std::string> patterns;
    std::optional<size_t> min_length;
    std::optional<size_t> max_length;
    int32_t priority = 0;
};

struct QueryClassificationConfig {
    bool enabled = false;
    std::vector<ClassificationRule> rules;
};

struct HeartbeatConfig {
    bool enabled = false;
    uint32_t interval_minutes = 30;
};

struct CronConfig {
    bool enabled = true;
    uint32_t max_run_history = 50;
};

struct CloudflareTunnelConfig {
    std::string token;
};

struct TailscaleTunnelConfig {
    bool funnel = false;
    std::optional<std::string> hostname;
};

struct NgrokTunnelConfig {
    std::string auth_token;
    std::optional<std::string> domain;
};

struct CustomTunnelConfig {
    std::string start_command;
    std::optional<std::string> health_url;
    std::optional<std::string> url_pattern;
};

struct TunnelConfig {
    std::string provider = "none";
    std::optional<CloudflareTunnelConfig> cloudflare;
    std::optional<TailscaleTunnelConfig> tailscale;
    std::optional<NgrokTunnelConfig> ngrok;
    std::optional<CustomTunnelConfig> custom;
};

struct TelegramConfig {
    std::string bot_token;
    std::vector<std::string> allowed_users;
    StreamMode stream_mode = StreamMode::Off;
    uint64_t draft_update_interval_ms = 1000;
    bool interrupt_on_new_message = false;
    bool mention_only = false;
};

struct DiscordConfig {
    std::string bot_token;
    std::optional<std::string> guild_id;
    std::vector<std::string> allowed_users;
    bool listen_to_bots = false;
    bool mention_only = false;
};

struct SlackConfig {
    std::string bot_token;
    std::optional<std::string> app_token;
    std::optional<std::string> channel_id;
    std::vector<std::string> allowed_users;
};

struct MattermostConfig {
    std::string url;
    std::string bot_token;
    std::optional<std::string> channel_id;
    std::vector<std::string> allowed_users;
    std::optional<bool> thread_replies;
    std::optional<bool> mention_only;
};

struct WebhookConfig {
    uint16_t port;
    std::optional<std::string> secret;
};

struct IMessageConfig {
    std::vector<std::string> allowed_contacts;
};

struct MatrixConfig {
    std::string homeserver;
    std::string access_token;
    std::optional<std::string> user_id;
    std::optional<std::string> device_id;
    std::string room_id;
    std::vector<std::string> allowed_users;
};

struct SignalConfig {
    std::string http_url;
    std::string account;
    std::optional<std::string> group_id;
    std::vector<std::string> allowed_from;
    bool ignore_attachments = false;
    bool ignore_stories = false;
};

struct WhatsAppConfig {
    std::optional<std::string> access_token;
    std::optional<std::string> phone_number_id;
    std::optional<std::string> verify_token;
    std::optional<std::string> app_secret;
    std::optional<std::string> session_path;
    std::optional<std::string> pair_phone;
    std::optional<std::string> pair_code;
    std::vector<std::string> allowed_numbers;

    const char* backend_type() const;
    bool is_cloud_config() const;
    bool is_web_config() const;
    bool is_ambiguous_config() const;
};

struct LinqConfig {
    std::string api_token;
    std::string from_phone;
    std::optional<std::string> signing_secret;
    std::vector<std::string> allowed_senders;
};

struct NextcloudTalkConfig {
    std::string base_url;
    std::string app_token;
    std::optional<std::string> webhook_secret;
    std::vector<std::string> allowed_users;
};

struct IrcConfig {
    std::string server;
    uint16_t port = 6697;
    std::string nickname;
    std::optional<std::string> username;
    std::vector<std::string> channels;
    std::vector<std::string> allowed_users;
    std::optional<std::string> server_password;
    std::optional<std::string> nickserv_password;
    std::optional<std::string> sasl_password;
    std::optional<bool> verify_tls;
};

struct LarkConfig {
    std::string app_id;
    std::string app_secret;
    std::optional<std::string> encrypt_key;
    std::optional<std::string> verification_token;
    std::vector<std::string> allowed_users;
    bool use_feishu = false;
    LarkReceiveMode receive_mode = LarkReceiveMode::Websocket;
    std::optional<uint16_t> port;
};

struct FeishuConfig {
    std::string app_id;
    std::string app_secret;
    std::optional<std::string> encrypt_key;
    std::optional<std::string> verification_token;
    std::vector<std::string> allowed_users;
    LarkReceiveMode receive_mode = LarkReceiveMode::Websocket;
    std::optional<uint16_t> port;
};

struct DingTalkConfig {
    std::string client_id;
    std::string client_secret;
    std::vector<std::string> allowed_users;
};

struct QQConfig {
    std::string app_id;
    std::string app_secret;
    std::vector<std::string> allowed_users;
};

struct NostrConfig {
    std::string private_key;
    std::vector<std::string> relays;
    std::vector<std::string> allowed_pubkeys;
};

struct ResourceLimitsConfig {
    uint32_t max_memory_mb = 512;
    uint64_t max_cpu_time_seconds = 60;
    uint32_t max_subprocesses = 10;
    bool memory_monitoring = true;
};

struct AuditConfig {
    bool enabled = true;
    std::string log_path = "audit.log";
    uint32_t max_size_mb = 100;
    bool sign_events = false;
};

struct OtpConfig {
    bool enabled = false;
    OtpMethod method = OtpMethod::Totp;
    uint64_t token_ttl_secs = 30;
    uint64_t cache_valid_secs = 300;
    std::vector<std::string> gated_actions;
    std::vector<std::string> gated_domains;
    std::vector<std::string> gated_domain_categories;
};

struct EstopConfig {
    bool enabled = false;
    std::string state_file = "~/.zeroclaw/estop-state.json";
    bool require_otp_to_resume = true;
};

struct SandboxConfig {
    std::optional<bool> enabled;
    SandboxBackend backend = SandboxBackend::Auto;
    std::vector<std::string> firejail_args;
};

struct SecurityConfig {
    SandboxConfig sandbox;
    ResourceLimitsConfig resources;
    AuditConfig audit;
    OtpConfig otp;
    EstopConfig estop;
};

struct ChannelsConfig {
    bool cli = true;
    std::optional<TelegramConfig> telegram;
    std::optional<DiscordConfig> discord;
    std::optional<SlackConfig> slack;
    std::optional<MattermostConfig> mattermost;
    std::optional<WebhookConfig> webhook;
    std::optional<IMessageConfig> imessage;
    std::optional<MatrixConfig> matrix;
    std::optional<SignalConfig> signal;
    std::optional<WhatsAppConfig> whatsapp;
    std::optional<LinqConfig> linq;
    std::optional<NextcloudTalkConfig> nextcloud_talk;
    std::optional<IrcConfig> irc;
    std::optional<LarkConfig> lark;
    std::optional<FeishuConfig> feishu;
    std::optional<DingTalkConfig> dingtalk;
    std::optional<QQConfig> qq;
    std::optional<NostrConfig> nostr;
    uint64_t message_timeout_secs = 300;
};

struct Config {
    std::filesystem::path workspace_dir;
    std::filesystem::path config_path;
    std::optional<std::string> api_key;
    std::optional<std::string> api_url;
    std::optional<std::string> default_provider;
    std::optional<std::string> default_model;
    double default_temperature = 0.7;

    ObservabilityConfig observability;
    AutonomyConfig autonomy;
    SecurityConfig security;
    RuntimeConfig runtime;
    ReliabilityConfig reliability;
    SchedulerConfig scheduler;
    AgentConfig agent;
    SkillsConfig skills;
    std::vector<ModelRouteConfig> model_routes;
    std::vector<EmbeddingRouteConfig> embedding_routes;
    QueryClassificationConfig query_classification;
    HeartbeatConfig heartbeat;
    CronConfig cron;
    ChannelsConfig channels_config;
    MemoryConfig memory;
    StorageConfig storage;
    TunnelConfig tunnel;
    GatewayConfig gateway;
    ComposioConfig composio;
    SecretsConfig secrets;
    BrowserConfig browser;
    HttpRequestConfig http_request;
    MultimodalConfig multimodal;
    WebSearchConfig web_search;
    ProxyConfig proxy;
    IdentityConfig identity;
    CostConfig cost;
    PeripheralsConfig peripherals;
    std::unordered_map<std::string, DelegateAgentConfig> agents;
    HooksConfig hooks;
    HardwareConfig hardware;
    TranscriptionConfig transcription;

    static Config create_default();
    void apply_env_overrides();
    bool validate() const;

    // TODO: Implement TOML serialization for C++
    std::string to_toml() const { return "# TOML serialization currently unimplemented"; }
    static std::optional<Config> from_toml(const std::string& toml) { return std::nullopt; }
    bool save() const { return false; }
};

std::unordered_map<std::string, ModelPricing> get_default_pricing();
std::vector<std::string> default_nostr_relays();

std::pair<std::string, bool> name_and_presence(const std::optional<TelegramConfig>& channel);
std::pair<std::string, bool> name_and_presence(const std::optional<DiscordConfig>& channel);

template<typename T>
std::pair<const char*, bool> name_and_presence_opt(const std::optional<T>& channel) {
    return {T::name(), channel.has_value()};
}

}
}
