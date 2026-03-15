#pragma once

/// HTTP transport layer — unified HTTP client for all outbound requests.
///
/// Wraps cpp-httplib to provide a simple, thread-safe HTTP client with:
///  - GET / POST / PUT / PATCH / DELETE methods
///  - JSON request/response convenience helpers
///  - Bearer token and API key authentication
///  - Per-host connection caching (httplib creates one client per host:port)
///  - Configurable timeouts and proxy support

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <mutex>
#include <unordered_map>
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace http {

// ── HTTP Response ────────────────────────────────────────────────

struct HttpResponse {
    int status = 0;
    std::string body;
    std::map<std::string, std::string> headers;

    /// True if status is 2xx
    bool ok() const { return status >= 200 && status < 300; }

    /// Parse body as JSON (returns null on parse error)
    nlohmann::json json() const;

    /// Human-readable error string for non-2xx
    std::string error_message() const;
};

// ── HTTP Request Options ─────────────────────────────────────────

struct RequestOptions {
    /// Additional headers to send
    std::map<std::string, std::string> headers;

    /// Request timeout in seconds (0 = use client default)
    int timeout_secs = 0;

    /// Content type (default: application/json for JSON methods)
    std::string content_type;
};

// ── HTTP Client ──────────────────────────────────────────────────

/// Unified HTTP client for all ZeroClaw++ outbound requests.
///
/// Thread-safe. Caches connections per host:port.
/// Supports HTTPS via OpenSSL (linked at build time).
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(HttpClient&&) = default;
    HttpClient& operator=(HttpClient&&) = default;

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // ── Configuration ────────────────────────────────────────

    /// Set a default Bearer token for all requests
    HttpClient& with_bearer_token(const std::string& token);

    /// Set a default API key header (e.g. x-api-key)
    HttpClient& with_api_key(const std::string& header_name, const std::string& key);

    /// Set default headers applied to every request
    HttpClient& with_default_headers(const std::map<std::string, std::string>& headers);

    /// Add a single default header
    HttpClient& with_header(const std::string& name, const std::string& value);

    /// Set default timeout in seconds
    HttpClient& with_timeout(int seconds);

    /// Set proxy URL (http://host:port or socks5://host:port)
    HttpClient& with_proxy(const std::string& proxy_url);

    // ── HTTP Methods ─────────────────────────────────────────

    /// GET request
    HttpResponse get(const std::string& url,
                     const RequestOptions& opts = {}) const;

    /// POST with raw body
    HttpResponse post(const std::string& url,
                      const std::string& body,
                      const std::string& content_type = "application/json",
                      const RequestOptions& opts = {}) const;

    /// POST with JSON body
    HttpResponse post_json(const std::string& url,
                           const nlohmann::json& body,
                           const RequestOptions& opts = {}) const;

    /// PUT with raw body
    HttpResponse put(const std::string& url,
                     const std::string& body,
                     const std::string& content_type = "application/json",
                     const RequestOptions& opts = {}) const;

    /// PATCH with raw body
    HttpResponse patch(const std::string& url,
                       const std::string& body,
                       const std::string& content_type = "application/json",
                       const RequestOptions& opts = {}) const;

    /// DELETE request
    HttpResponse delete_(const std::string& url,
                         const RequestOptions& opts = {}) const;

    /// POST form-encoded data
    HttpResponse post_form(const std::string& url,
                           const std::map<std::string, std::string>& form_data,
                           const RequestOptions& opts = {}) const;

    // ── JSON Convenience ─────────────────────────────────────

    /// GET and parse response as JSON
    std::pair<nlohmann::json, HttpResponse> get_json(const std::string& url,
                                                      const RequestOptions& opts = {}) const;

private:
    struct ParsedUrl {
        std::string scheme;   // "http" or "https"
        std::string host;
        int port = 0;
        std::string path;
    };

    /// Parse a URL into components
    static ParsedUrl parse_url(const std::string& url);

    /// Build the merged headers for a request
    std::map<std::string, std::string> merged_headers(const RequestOptions& opts) const;

    /// Get or create an httplib client for the given host:port
    struct ClientEntry;
    ClientEntry& get_client(const std::string& scheme,
                            const std::string& host,
                            int port) const;

    // Default configuration
    std::map<std::string, std::string> default_headers_;
    int default_timeout_secs_ = 30;
    std::string proxy_host_;
    int proxy_port_ = 0;

    // Connection cache
    mutable std::unique_ptr<std::mutex> cache_mutex_;
    mutable std::unordered_map<std::string, std::unique_ptr<ClientEntry>> client_cache_;
};

// ── Factory ──────────────────────────────────────────────────────

/// Create a basic HTTP client
HttpClient create_client();

/// Create an HTTP client with Bearer token auth
HttpClient create_client_with_bearer(const std::string& token);

/// Create an HTTP client with custom API key header
HttpClient create_client_with_api_key(const std::string& header_name,
                                       const std::string& key);

} // namespace http
} // namespace zeroclaw
