#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace zeroclaw::onboard::wizard {

namespace fs = std::filesystem;

struct ProjectContext {
    std::string user_name;
    std::string timezone;
    std::string agent_name;
    std::string communication_style;
    
    ProjectContext() = default;
    ProjectContext(std::string user, std::string tz, std::string agent, std::string style)
        : user_name(std::move(user)), timezone(std::move(tz)), 
          agent_name(std::move(agent)), communication_style(std::move(style)) {}
};

struct MemoryConfig {
    std::string backend = "sqlite";
    bool auto_save = true;
    bool hygiene_enabled = true;
    int archive_after_days = 7;
    int purge_after_days = 30;
    int conversation_retention_days = 30;
    std::string embedding_provider = "none";
    std::string embedding_model = "text-embedding-3-small";
    int embedding_dimensions = 1536;
    double vector_weight = 0.7;
    double keyword_weight = 0.3;
    double min_relevance_score = 0.4;
    int embedding_cache_size = 10000;
    int chunk_max_tokens = 512;
    bool response_cache_enabled = false;
    int response_cache_ttl_minutes = 60;
    int response_cache_max_entries = 5000;
    bool snapshot_enabled = false;
    bool snapshot_on_hygiene = false;
    bool auto_hydrate = true;
    std::optional<int> sqlite_open_timeout_secs;
};

struct TelegramConfig {
    std::string bot_token;
    std::vector<std::string> allowed_users;
    std::string stream_mode = "text";
    int draft_update_interval_ms = 1000;
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
    bool ignore_stories = true;
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
};

struct LinqConfig {
    std::string api_token;
    std::string from_phone;
    std::optional<std::string> signing_secret;
    std::vector<std::string> allowed_senders;
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

struct WebhookConfig {
    uint16_t port = 8080;
    std::optional<std::string> secret;
};

struct NextcloudTalkConfig {
    std::string base_url;
    std::string app_token;
    std::optional<std::string> webhook_secret;
    std::vector<std::string> allowed_users;
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

enum class LarkReceiveMode { Websocket, Webhook };

struct LarkConfig {
    std::string app_id;
    std::string app_secret;
    std::optional<std::string> verification_token;
    std::optional<std::string> encrypt_key;
    std::vector<std::string> allowed_users;
    bool use_feishu = false;
    LarkReceiveMode receive_mode = LarkReceiveMode::Websocket;
    std::optional<uint16_t> port;
};

struct FeishuConfig {
    std::string app_id;
    std::string app_secret;
    std::optional<std::string> verification_token;
    std::optional<std::string> encrypt_key;
    std::vector<std::string> allowed_users;
    LarkReceiveMode receive_mode = LarkReceiveMode::Websocket;
    std::optional<uint16_t> port;
};

struct NostrConfig {
    std::string private_key;
    std::vector<std::string> relays;
    std::vector<std::string> allowed_pubkeys;
};

struct ChannelsConfig {
    std::optional<TelegramConfig> telegram;
    std::optional<DiscordConfig> discord;
    std::optional<SlackConfig> slack;
    std::optional<IMessageConfig> imessage;
    std::optional<MatrixConfig> matrix;
    std::optional<SignalConfig> signal;
    std::optional<WhatsAppConfig> whatsapp;
    std::optional<LinqConfig> linq;
    std::optional<IrcConfig> irc;
    std::optional<WebhookConfig> webhook;
    std::optional<NextcloudTalkConfig> nextcloud_talk;
    std::optional<DingTalkConfig> dingtalk;
    std::optional<QQConfig> qq;
    std::optional<LarkConfig> lark;
    std::optional<FeishuConfig> feishu;
    std::optional<NostrConfig> nostr;
    bool cli = true;
};

struct ComposioConfig {
    bool enabled = false;
    std::optional<std::string> api_key;
};

struct SecretsConfig {
    bool encrypt = true;
};

struct TunnelConfig {
    std::string provider = "none";
    std::optional<std::string> cloudflare_token;
    std::optional<std::string> ngrok_auth_token;
    std::optional<std::string> ngrok_domain;
    bool tailscale_funnel = false;
    std::optional<std::string> custom_start_command;
};

struct HardwareConfig {
    bool enabled = false;
    std::string transport = "none";
    std::optional<std::string> serial_port;
    uint32_t baud_rate = 115200;
    std::optional<std::string> probe_target;
    bool workspace_datasheets = false;
};

struct ObservabilityConfig {
    bool enabled = false;
    std::string provider = "none";
};

struct AutonomyConfig {
    std::string level = "supervised";
};

struct SecurityConfig {
    bool sandbox = true;
};

struct RuntimeConfig {
    std::string mode = "native";
};

struct ReliabilityConfig {
    int max_retries = 3;
    int timeout_secs = 120;
};

struct SchedulerConfig {
    bool enabled = false;
};

struct AgentConfig {
    std::string name = "ZeroClaw";
};

struct SkillsConfig {
    bool open_skills_enabled = false;
};

struct HeartbeatConfig {
    bool enabled = false;
    int interval_secs = 60;
};

struct CronConfig {
    bool enabled = false;
};

struct StorageConfig {
    std::string backend = "filesystem";
};

struct GatewayConfig {
    bool require_pairing = true;
    uint16_t port = 8080;
};

struct BrowserConfig {
    bool enabled = false;
    std::string headless = "true";
};

struct HttpRequestConfig {
    int timeout_secs = 30;
};

struct MultimodalConfig {
    bool enabled = false;
};

struct WebSearchConfig {
    bool enabled = false;
};

struct ProxyConfig {
    std::optional<std::string> http;
    std::optional<std::string> https;
};

struct IdentityConfig {
    std::optional<std::string> name;
};

struct CostConfig {
    bool tracking_enabled = false;
};

struct PeripheralsConfig {
    bool enabled = false;
};

struct HooksConfig {
    bool enabled = false;
};

struct QueryClassificationConfig {
    bool enabled = false;
};

struct TranscriptionConfig {
    bool enabled = false;
};

struct Config {
    fs::path workspace_dir;
    fs::path config_path;
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
    std::vector<std::string> model_routes;
    std::vector<std::string> embedding_routes;
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
    std::map<std::string, std::string> agents;
    HooksConfig hooks;
    HardwareConfig hardware;
    QueryClassificationConfig query_classification;
    TranscriptionConfig transcription;
    
    void save() const;
    static Config load_or_init();
    static Config load_from_file(const fs::path& path);
};

struct ModelCacheEntry {
    std::string provider;
    uint64_t fetched_at_unix;
    std::vector<std::string> models;
};

struct ModelCacheState {
    std::vector<ModelCacheEntry> entries;
};

struct CachedModels {
    std::vector<std::string> models;
    uint64_t age_secs;
};

struct MemoryBackendProfile {
    bool auto_save_default = true;
    bool uses_sqlite_hygiene = false;
    bool sqlite_based = false;
    bool optional_dependency = false;
};

struct MemoryBackend {
    std::string key;
    std::string label;
};

class WizardError : public std::runtime_error {
public:
    explicit WizardError(const std::string& msg) : std::runtime_error(msg) {}
};

Config run_wizard(bool force = false);
Config run_channels_repair_wizard();
Config run_quick_setup(
    std::optional<std::string> credential_override = std::nullopt,
    std::optional<std::string> provider = std::nullopt,
    std::optional<std::string> model_override = std::nullopt,
    std::optional<std::string> memory_backend = std::nullopt,
    bool force = false
);
void run_models_refresh(
    const Config& config,
    std::optional<std::string> provider_override = std::nullopt,
    bool force = false
);

std::string canonical_provider_name(const std::string& provider_name);
std::string default_model_for_provider(const std::string& provider);
std::vector<std::pair<std::string, std::string>> curated_models_for_provider(const std::string& provider_name);
bool supports_live_model_fetch(const std::string& provider_name);
bool allows_unauthenticated_model_fetch(const std::string& provider_name);
std::optional<std::string> models_endpoint_for_provider(const std::string& provider_name);
std::string provider_env_var(const std::string& name);
bool provider_supports_keyless_local_usage(const std::string& provider_name);

std::vector<MemoryBackend> selectable_memory_backends();
std::string default_memory_backend_key();
MemoryBackendProfile memory_backend_profile(const std::string& backend);

void print_step(uint8_t current, uint8_t total, const std::string& title);
void print_bullet(const std::string& text);

std::vector<std::string> normalize_model_ids(std::vector<std::string> ids);
std::vector<std::string> parse_openai_compatible_model_ids(const std::string& json_payload);
std::vector<std::string> parse_gemini_model_ids(const std::string& json_payload);
std::vector<std::string> parse_ollama_model_ids(const std::string& json_payload);

std::string humanize_age(uint64_t age_secs);
uint64_t now_unix_secs();

std::vector<std::pair<std::string, std::string>> local_provider_choices();

bool has_launchable_channels(const ChannelsConfig& channels);

}
