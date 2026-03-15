#include "config.hpp"
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <cctype>
#include <mutex>

namespace zeroclaw {
namespace config {

static const std::vector<std::string> SUPPORTED_PROXY_SERVICE_KEYS = {
    "provider.anthropic",
    "provider.compatible",
    "provider.copilot",
    "provider.gemini",
    "provider.glm",
    "provider.ollama",
    "provider.openai",
    "provider.openrouter",
    "channel.dingtalk",
    "channel.discord",
    "channel.feishu",
    "channel.lark",
    "channel.matrix",
    "channel.mattermost",
    "channel.nextcloud_talk",
    "channel.qq",
    "channel.signal",
    "channel.slack",
    "channel.telegram",
    "channel.whatsapp",
    "tool.browser",
    "tool.composio",
    "tool.http_request",
    "tool.pushover",
    "memory.embeddings",
    "tunnel.custom",
    "transcription.groq",
};

static const std::vector<std::string> SUPPORTED_PROXY_SERVICE_SELECTORS = {
    "provider.*",
    "channel.*",
    "tool.*",
    "memory.*",
    "tunnel.*",
    "transcription.*",
};

static std::string to_lower(const std::string& s) {
    std::string result = s;
    for (auto& c : result) c = std::tolower(static_cast<unsigned char>(c));
    return result;
}

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

static std::optional<std::string> normalize_proxy_url_opt(const std::optional<std::string>& raw) {
    if (!raw.has_value()) return std::nullopt;
    std::string value = trim(*raw);
    if (value.empty()) return std::nullopt;
    return value;
}

static std::vector<std::string> normalize_comma_values(const std::vector<std::string>& values) {
    std::vector<std::string> output;
    for (const auto& value : values) {
        std::istringstream ss(value);
        std::string part;
        while (std::getline(ss, part, ',')) {
            std::string normalized = trim(part);
            if (!normalized.empty()) {
                output.push_back(normalized);
            }
        }
    }
    std::sort(output.begin(), output.end());
    output.erase(std::unique(output.begin(), output.end()), output.end());
    return output;
}

static bool is_supported_proxy_service_selector(const std::string& selector) {
    for (const auto& key : SUPPORTED_PROXY_SERVICE_KEYS) {
        if (to_lower(key) == to_lower(selector)) return true;
    }
    for (const auto& sel : SUPPORTED_PROXY_SERVICE_SELECTORS) {
        if (to_lower(sel) == to_lower(selector)) return true;
    }
    return false;
}

static bool service_selector_matches(const std::string& selector, const std::string& service_key) {
    if (selector == service_key) return true;
    if (selector.size() >= 2 && selector.substr(selector.size() - 2) == ".*") {
        std::string prefix = selector.substr(0, selector.size() - 2);
        if (service_key.size() > prefix.size() &&
            service_key.substr(0, prefix.size()) == prefix &&
            service_key[prefix.size()] == '.') {
            return true;
        }
    }
    return false;
}

HardwareTransport HardwareConfig::transport_mode() const {
    return transport;
}

std::pair<size_t, size_t> MultimodalConfig::effective_limits() const {
    size_t max_images = std::clamp(max_images, size_t(1), size_t(16));
    size_t max_image_size_mb_clamped = std::clamp(max_image_size_mb, size_t(1), size_t(20));
    return {max_images, max_image_size_mb_clamped};
}

const char* WhatsAppConfig::backend_type() const {
    if (phone_number_id.has_value()) return "cloud";
    if (session_path.has_value()) return "web";
    return "cloud";
}

bool WhatsAppConfig::is_cloud_config() const {
    return phone_number_id.has_value() && access_token.has_value() && verify_token.has_value();
}

bool WhatsAppConfig::is_web_config() const {
    return session_path.has_value();
}

bool WhatsAppConfig::is_ambiguous_config() const {
    return phone_number_id.has_value() && session_path.has_value();
}

const std::vector<std::string>& ProxyConfig::supported_service_keys() {
    return SUPPORTED_PROXY_SERVICE_KEYS;
}

const std::vector<std::string>& ProxyConfig::supported_service_selectors() {
    return SUPPORTED_PROXY_SERVICE_SELECTORS;
}

bool ProxyConfig::has_any_proxy_url() const {
    return normalize_proxy_url_opt(http_proxy).has_value() ||
           normalize_proxy_url_opt(https_proxy).has_value() ||
           normalize_proxy_url_opt(all_proxy).has_value();
}

std::vector<std::string> ProxyConfig::normalized_services() const {
    auto normalized = normalize_comma_values(services);
    for (auto& s : normalized) {
        s = to_lower(s);
    }
    std::sort(normalized.begin(), normalized.end());
    normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());
    return normalized;
}

std::vector<std::string> ProxyConfig::normalized_no_proxy() const {
    return normalize_comma_values(no_proxy);
}

bool ProxyConfig::should_apply_to_service(const std::string& service_key) const {
    if (!enabled) return false;
    switch (scope) {
        case ProxyScope::Environment:
            return false;
        case ProxyScope::Zeroclaw:
            return true;
        case ProxyScope::Services: {
            std::string key = to_lower(trim(service_key));
            if (key.empty()) return false;
            for (const auto& selector : normalized_services()) {
                if (service_selector_matches(selector, key)) return true;
            }
            return false;
        }
    }
    return false;
}

std::unordered_map<std::string, ModelPricing> get_default_pricing() {
    std::unordered_map<std::string, ModelPricing> prices;
    prices["anthropic/claude-sonnet-4-20250514"] = {3.0, 15.0};
    prices["anthropic/claude-opus-4-20250514"] = {15.0, 75.0};
    prices["anthropic/claude-3.5-sonnet"] = {3.0, 15.0};
    prices["anthropic/claude-3-haiku"] = {0.25, 1.25};
    prices["openai/gpt-4o"] = {5.0, 15.0};
    prices["openai/gpt-4o-mini"] = {0.15, 0.60};
    prices["openai/o1-preview"] = {15.0, 60.0};
    prices["google/gemini-2.0-flash"] = {0.10, 0.40};
    prices["google/gemini-1.5-pro"] = {1.25, 5.0};
    return prices;
}

std::vector<std::string> default_nostr_relays() {
    return {
        "wss://relay.damus.io",
        "wss://nos.lol",
        "wss://relay.primal.net",
        "wss://relay.snort.social",
    };
}

Config Config::create_default() {
    Config config;
    
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    std::filesystem::path home_path = home ? home : ".";
    std::filesystem::path zeroclaw_dir = home_path / ".zeroclaw";
    
    config.workspace_dir = zeroclaw_dir / "workspace";
    config.config_path = zeroclaw_dir / "config.toml";
    config.default_provider = "openrouter";
    config.default_model = "anthropic/claude-sonnet-4.6";
    config.default_temperature = 0.7;
    config.cost.prices = get_default_pricing();
    
    config.autonomy.allowed_commands = {
        "git", "npm", "cargo", "ls", "cat", "grep", "find",
        "echo", "pwd", "wc", "head", "tail", "date"
    };
    config.autonomy.forbidden_paths = {
        "/etc", "/root", "/home", "/usr", "/bin", "/sbin",
        "/lib", "/opt", "/boot", "/dev", "/proc", "/sys",
        "/var", "/tmp", "~/.ssh", "~/.gnupg", "~/.aws", "~/.config"
    };
    config.autonomy.auto_approve = {"file_read", "memory_recall"};
    
    config.security.otp.gated_actions = {
        "shell", "file_write", "browser_open", "browser", "memory_forget"
    };
    
    return config;
}

Config Config::load_or_init(const std::string& config_dir) {
    Config config = create_default();
    if (!config_dir.empty()) {
        std::filesystem::path dir(config_dir);
        config.config_path = dir / "config.toml";
        config.workspace_dir = dir / "workspace";
    }
    // Note: TOML loading is currently stubbed out in from_toml. 
    // This just maps the defaults and paths according to Rust's fallback logic.
    return config;
}

void Config::apply_env_overrides() {
    const char* val = nullptr;
    
    if ((val = std::getenv("ZEROCLAW_API_KEY")) || (val = std::getenv("API_KEY"))) {
        if (val && *val) api_key = val;
    }
    
    if ((val = std::getenv("ZEROCLAW_PROVIDER"))) {
        if (val && *val) default_provider = val;
    } else if ((val = std::getenv("PROVIDER"))) {
        if (val && *val && (!default_provider || to_lower(*default_provider) == "openrouter")) {
            default_provider = val;
        }
    }
    
    if ((val = std::getenv("ZEROCLAW_MODEL")) || (val = std::getenv("MODEL"))) {
        if (val && *val) default_model = val;
    }
    
    if ((val = std::getenv("ZEROCLAW_WORKSPACE"))) {
        if (val && *val) {
            workspace_dir = std::filesystem::path(val);
        }
    }
    
    if ((val = std::getenv("ZEROCLAW_GATEWAY_PORT")) || (val = std::getenv("PORT"))) {
        if (val && *val) {
            try {
                gateway.port = static_cast<uint16_t>(std::stoi(val));
            } catch (...) {}
        }
    }
    
    if ((val = std::getenv("ZEROCLAW_GATEWAY_HOST")) || (val = std::getenv("HOST"))) {
        if (val && *val) gateway.host = val;
    }
    
    if ((val = std::getenv("ZEROCLAW_ALLOW_PUBLIC_BIND"))) {
        gateway.allow_public_bind = (std::string(val) == "1" || to_lower(val) == "true");
    }
    
    if ((val = std::getenv("ZEROCLAW_TEMPERATURE"))) {
        if (val && *val) {
            try {
                double temp = std::stod(val);
                if (temp >= 0.0 && temp <= 2.0) {
                    default_temperature = temp;
                }
            } catch (...) {}
        }
    }
    
    if ((val = std::getenv("ZEROCLAW_PROXY_ENABLED"))) {
        std::string enabled = to_lower(trim(val));
        if (enabled == "1" || enabled == "true" || enabled == "yes" || enabled == "on") {
            proxy.enabled = true;
        } else if (enabled == "0" || enabled == "false" || enabled == "no" || enabled == "off") {
            proxy.enabled = false;
        }
    }
    
    if ((val = std::getenv("HTTP_PROXY"))) {
        proxy.http_proxy = normalize_proxy_url_opt(std::string(val));
    }
    if ((val = std::getenv("HTTPS_PROXY"))) {
        proxy.https_proxy = normalize_proxy_url_opt(std::string(val));
    }
    if ((val = std::getenv("ALL_PROXY"))) {
        proxy.all_proxy = normalize_proxy_url_opt(std::string(val));
    }
    if ((val = std::getenv("NO_PROXY"))) {
        proxy.no_proxy = normalize_comma_values({val});
    }
    
    if ((val = std::getenv("ZEROCLAW_WEB_SEARCH_ENABLED")) || (val = std::getenv("WEB_SEARCH_ENABLED"))) {
        web_search.enabled = (std::string(val) == "1" || to_lower(val) == "true");
    }
    
    if ((val = std::getenv("ZEROCLAW_STORAGE_PROVIDER"))) {
        if (val && *val) storage.provider.config.provider = val;
    }
    
    if ((val = std::getenv("ZEROCLAW_STORAGE_DB_URL"))) {
        if (val && *val) storage.provider.config.db_url = val;
    }
}

bool Config::validate() const {
    if (gateway.host.empty() || trim(gateway.host).empty()) {
        return false;
    }
    if (autonomy.max_actions_per_hour == 0) {
        return false;
    }
    if (scheduler.max_concurrent == 0) {
        return false;
    }
    if (scheduler.max_tasks == 0) {
        return false;
    }
    for (const auto& route : model_routes) {
        if (trim(route.hint).empty() || trim(route.provider).empty() || trim(route.model).empty()) {
            return false;
        }
    }
    for (const auto& route : embedding_routes) {
        if (trim(route.hint).empty() || trim(route.provider).empty() || trim(route.model).empty()) {
            return false;
        }
    }
    if (security.otp.token_ttl_secs == 0 || security.otp.cache_valid_secs == 0) {
        return false;
    }
    if (security.otp.cache_valid_secs < security.otp.token_ttl_secs) {
        return false;
    }
    return true;
}

std::pair<std::string, bool> name_and_presence(const std::optional<TelegramConfig>& channel) {
    return {"Telegram", channel.has_value()};
}

std::pair<std::string, bool> name_and_presence(const std::optional<DiscordConfig>& channel) {
    return {"Discord", channel.has_value()};
}

}
}
