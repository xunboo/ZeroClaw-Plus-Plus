#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace zeroclaw::gateway {

inline constexpr size_t MAX_BODY_SIZE = 65536;
inline constexpr uint64_t REQUEST_TIMEOUT_SECS = 30;
inline constexpr uint64_t RATE_LIMIT_WINDOW_SECS = 60;
inline constexpr size_t RATE_LIMIT_MAX_KEYS_DEFAULT = 10000;
inline constexpr size_t IDEMPOTENCY_MAX_KEYS_DEFAULT = 10000;
inline constexpr uint64_t RATE_LIMITER_SWEEP_INTERVAL_SECS = 300;

std::string hash_webhook_secret(const std::string& value);
std::string webhook_memory_key();
std::string whatsapp_memory_key(const std::string& sender, const std::string& id);
std::string linq_memory_key(const std::string& sender, const std::string& id);
std::string nextcloud_talk_memory_key(const std::string& sender, const std::string& id);

class SlidingWindowRateLimiter {
public:
    SlidingWindowRateLimiter(uint32_t limit_per_window, 
                             std::chrono::seconds window, 
                             size_t max_keys);

    bool allow(const std::string& key);

private:
    void prune_stale(std::chrono::steady_clock::time_point cutoff);

    uint32_t limit_per_window_;
    std::chrono::seconds window_;
    size_t max_keys_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>> requests_;
    std::chrono::steady_clock::time_point last_sweep_;
};

class GatewayRateLimiter {
public:
    GatewayRateLimiter(uint32_t pair_per_minute, 
                       uint32_t webhook_per_minute, 
                       size_t max_keys);

    bool allow_pair(const std::string& key);
    bool allow_webhook(const std::string& key);

private:
    SlidingWindowRateLimiter pair_;
    SlidingWindowRateLimiter webhook_;
};

class IdempotencyStore {
public:
    IdempotencyStore(std::chrono::seconds ttl, size_t max_keys);

    bool record_if_new(const std::string& key);

private:
    std::chrono::seconds ttl_;
    size_t max_keys_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> keys_;
};

struct IpAddr {
    std::string value;
    
    static std::optional<IpAddr> parse(const std::string& value);
};

struct SocketAddr {
    IpAddr ip;
    uint16_t port;
    
    static std::optional<SocketAddr> parse(const std::string& value);
};

namespace http {

struct HeaderMap {
    std::unordered_map<std::string, std::string> headers;
    
    std::optional<std::string> get(const std::string& key) const;
    void set(const std::string& key, const std::string& value);
};

enum class StatusCode : uint16_t {
    OK = 200,
    CREATED = 201,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    TOO_MANY_REQUESTS = 429,
    INTERNAL_SERVER_ERROR = 500,
    REQUEST_TIMEOUT = 408,
};

struct Response {
    StatusCode status_code;
    HeaderMap headers;
    std::string body;
    std::string content_type;
};

struct Request {
    std::string method;
    std::string path;
    HeaderMap headers;
    std::string body;
    std::optional<SocketAddr> peer_addr;
};

using Handler = std::function<Response(const Request&)>;

} 
} // namespace zeroclaw::gateway
namespace zeroclaw::config {
struct Config;
}
namespace zeroclaw::gateway {

} // namespace zeroclaw::gateway

namespace zeroclaw::memory {
class Memory;
enum class MemoryCategory;
}

namespace zeroclaw::providers {
class Provider;
struct ChatMessage;
}

namespace zeroclaw::security {
class PairingGuard;
}

namespace zeroclaw::observability {
class Observer;
}

namespace zeroclaw::channels {
class WhatsAppChannel;
class LinqChannel;
class NextcloudTalkChannel;
struct SendMessage;
}
namespace zeroclaw {
struct ToolSpec;
}
namespace zeroclaw::cost {
class CostTracker;
}

namespace zeroclaw::gateway {

struct AppState {
    std::shared_ptr<config::Config> config;
    std::shared_ptr<providers::Provider> provider;
    std::string model;
    double temperature;
    std::shared_ptr<memory::Memory> mem;
    bool auto_save;
    std::optional<std::string> webhook_secret_hash;
    std::shared_ptr<security::PairingGuard> pairing;
    bool trust_forwarded_headers;
    std::shared_ptr<GatewayRateLimiter> rate_limiter;
    std::shared_ptr<IdempotencyStore> idempotency_store;
    std::shared_ptr<channels::WhatsAppChannel> whatsapp;
    std::optional<std::string> whatsapp_app_secret;
    std::shared_ptr<channels::LinqChannel> linq;
    std::optional<std::string> linq_signing_secret;
    std::shared_ptr<channels::NextcloudTalkChannel> nextcloud_talk;
    std::optional<std::string> nextcloud_talk_webhook_secret;
    std::shared_ptr<observability::Observer> observer;
    std::shared_ptr<std::vector<ToolSpec>> tools_registry;
    std::shared_ptr<cost::CostTracker> cost_tracker;
};

class IHttpServer {
public:
    virtual ~IHttpServer() = default;
    virtual void route(const std::string& method, const std::string& path, http::Handler handler) = 0;
    virtual void route_fallback(http::Handler handler) = 0;
    virtual void set_body_limit(size_t bytes) = 0;
    virtual void set_timeout(std::chrono::seconds timeout) = 0;
    virtual void run(const std::string& host, uint16_t port) = 0;
    virtual void stop() = 0;
};

class IWebSocketServer {
public:
    virtual ~IWebSocketServer() = default;
    virtual void on_upgrade(std::function<void(const std::string&)> on_message,
                           std::function<void()> on_close) = 0;
};

std::optional<IpAddr> forwarded_client_ip(const http::HeaderMap& headers);
std::string client_key_from_request(const std::optional<SocketAddr>& peer_addr,
                                    const http::HeaderMap& headers,
                                    bool trust_forwarded_headers);

size_t normalize_max_keys(size_t configured, size_t fallback);

http::Response handle_health(const AppState& state);
http::Response handle_metrics(const AppState& state);
http::Response handle_pair(AppState& state, const http::Request& req);
http::Response handle_webhook(AppState& state, const http::Request& req);
http::Response handle_whatsapp_verify(AppState& state, const http::Request& req);
http::Response handle_whatsapp_message(AppState& state, const http::Request& req);
http::Response handle_linq_webhook(AppState& state, const http::Request& req);
http::Response handle_nextcloud_talk_webhook(AppState& state, const http::Request& req);

bool verify_whatsapp_signature(const std::string& app_secret,
                               const std::vector<uint8_t>& body,
                               const std::string& signature_header);

std::optional<std::string> run_gateway(const std::string& host, 
                                       uint16_t port, 
                                       zeroclaw::config::Config config);

}
