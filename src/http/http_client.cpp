#include "http_client.hpp"

// cpp-httplib must be included AFTER defining CPPHTTPLIB_OPENSSL_SUPPORT
// to enable HTTPS via OpenSSL.
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "ccp-httplib/httplib.h"

#include <sstream>
#include <algorithm>
#include <cctype>

namespace zeroclaw {
namespace http {

// ── HttpResponse helpers ─────────────────────────────────────────

nlohmann::json HttpResponse::json() const {
    try {
        return nlohmann::json::parse(body);
    } catch (...) {
        return nlohmann::json();
    }
}

std::string HttpResponse::error_message() const {
    if (ok()) return "";
    std::ostringstream oss;
    oss << "HTTP " << status;
    if (!body.empty()) {
        // Try to extract error message from JSON body
        try {
            auto j = nlohmann::json::parse(body);
            if (j.contains("error")) {
                auto& err = j["error"];
                if (err.is_string()) {
                    oss << ": " << err.get<std::string>();
                } else if (err.is_object() && err.contains("message")) {
                    oss << ": " << err["message"].get<std::string>();
                }
            } else if (j.contains("message")) {
                oss << ": " << j["message"].get<std::string>();
            }
        } catch (...) {
            // Not JSON, include raw body (truncated)
            auto preview = body.substr(0, 200);
            oss << ": " << preview;
        }
    }
    return oss.str();
}

// ── URL parsing ──────────────────────────────────────────────────

HttpClient::ParsedUrl HttpClient::parse_url(const std::string& url) {
    ParsedUrl result;

    // Extract scheme
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        result.scheme = "https";
        scheme_end = 0;
    } else {
        result.scheme = url.substr(0, scheme_end);
        std::transform(result.scheme.begin(), result.scheme.end(),
                       result.scheme.begin(), ::tolower);
        scheme_end += 3;
    }

    // Find path start
    auto path_start = url.find('/', scheme_end);
    std::string host_port;
    if (path_start == std::string::npos) {
        host_port = url.substr(scheme_end);
        result.path = "/";
    } else {
        host_port = url.substr(scheme_end, path_start - scheme_end);
        result.path = url.substr(path_start);
    }

    // Split host:port
    auto colon = host_port.rfind(':');
    if (colon != std::string::npos && colon > 0) {
        result.host = host_port.substr(0, colon);
        try {
            result.port = std::stoi(host_port.substr(colon + 1));
        } catch (...) {
            result.host = host_port;
            result.port = 0;
        }
    } else {
        result.host = host_port;
        result.port = 0;
    }

    // Default ports
    if (result.port == 0) {
        result.port = (result.scheme == "https") ? 443 : 80;
    }

    return result;
}

// ── ClientEntry — wraps an httplib client ────────────────────────

struct HttpClient::ClientEntry {
    std::unique_ptr<httplib::Client> client;
    std::mutex mutex;
};

// ── HttpClient implementation ────────────────────────────────────

HttpClient::HttpClient() : cache_mutex_(std::make_unique<std::mutex>()) {}
HttpClient::~HttpClient() = default;

HttpClient& HttpClient::with_bearer_token(const std::string& token) {
    default_headers_["Authorization"] = "Bearer " + token;
    return *this;
}

HttpClient& HttpClient::with_api_key(const std::string& header_name,
                                      const std::string& key) {
    default_headers_[header_name] = key;
    return *this;
}

HttpClient& HttpClient::with_default_headers(
    const std::map<std::string, std::string>& headers) {
    for (const auto& [k, v] : headers) {
        default_headers_[k] = v;
    }
    return *this;
}

HttpClient& HttpClient::with_header(const std::string& name,
                                     const std::string& value) {
    default_headers_[name] = value;
    return *this;
}

HttpClient& HttpClient::with_timeout(int seconds) {
    default_timeout_secs_ = seconds;
    return *this;
}

HttpClient& HttpClient::with_proxy(const std::string& proxy_url) {
    auto parsed = parse_url(proxy_url);
    proxy_host_ = parsed.host;
    proxy_port_ = parsed.port;
    return *this;
}

// ── Connection caching ───────────────────────────────────────────

HttpClient::ClientEntry& HttpClient::get_client(
    const std::string& scheme,
    const std::string& host,
    int port) const {

    std::string cache_key = scheme + "://" + host + ":" + std::to_string(port);

    std::lock_guard<std::mutex> lock(*cache_mutex_);
    auto it = client_cache_.find(cache_key);
    if (it != client_cache_.end()) {
        return *it->second;
    }

    auto entry = std::make_unique<ClientEntry>();

    if (scheme == "https") {
        entry->client = std::make_unique<httplib::Client>(
            scheme + "://" + host + ":" + std::to_string(port));
    } else {
        entry->client = std::make_unique<httplib::Client>(
            host, port);
    }

    // Configure the client
    entry->client->set_connection_timeout(default_timeout_secs_, 0);
    entry->client->set_read_timeout(default_timeout_secs_, 0);
    entry->client->set_write_timeout(default_timeout_secs_, 0);
    entry->client->set_follow_location(true);

    // Proxy
    if (!proxy_host_.empty()) {
        entry->client->set_proxy(proxy_host_, proxy_port_);
    }

    auto [inserted_it, _] = client_cache_.emplace(cache_key, std::move(entry));
    return *inserted_it->second;
}

// ── Header merging ───────────────────────────────────────────────

std::map<std::string, std::string> HttpClient::merged_headers(
    const RequestOptions& opts) const {

    auto headers = default_headers_;
    for (const auto& [k, v] : opts.headers) {
        headers[k] = v;
    }
    return headers;
}

// ── Helper to convert response ───────────────────────────────────

static HttpResponse make_response(const httplib::Result& result) {
    HttpResponse resp;
    if (!result) {
        resp.status = 0;
        resp.body = "Connection error: " + httplib::to_string(result.error());
        return resp;
    }
    resp.status = result->status;
    resp.body = result->body;
    for (const auto& [k, v] : result->headers) {
        resp.headers[k] = v;
    }
    return resp;
}

static httplib::Headers to_httplib_headers(
    const std::map<std::string, std::string>& headers) {
    httplib::Headers h;
    for (const auto& [k, v] : headers) {
        h.emplace(k, v);
    }
    return h;
}

// ── GET ──────────────────────────────────────────────────────────

HttpResponse HttpClient::get(const std::string& url,
                              const RequestOptions& opts) const {
    auto parsed = parse_url(url);
    auto& entry = get_client(parsed.scheme, parsed.host, parsed.port);
    std::lock_guard<std::mutex> lock(entry.mutex);

    auto headers = merged_headers(opts);
    auto result = entry.client->Get(parsed.path, to_httplib_headers(headers));
    return make_response(result);
}

// ── POST ─────────────────────────────────────────────────────────

HttpResponse HttpClient::post(const std::string& url,
                               const std::string& body,
                               const std::string& content_type,
                               const RequestOptions& opts) const {
    auto parsed = parse_url(url);
    auto& entry = get_client(parsed.scheme, parsed.host, parsed.port);
    std::lock_guard<std::mutex> lock(entry.mutex);

    auto headers = merged_headers(opts);
    auto result = entry.client->Post(
        parsed.path, to_httplib_headers(headers), body, content_type);
    return make_response(result);
}

HttpResponse HttpClient::post_json(const std::string& url,
                                    const nlohmann::json& body,
                                    const RequestOptions& opts) const {
    return post(url, body.dump(), "application/json", opts);
}

// ── PUT ──────────────────────────────────────────────────────────

HttpResponse HttpClient::put(const std::string& url,
                              const std::string& body,
                              const std::string& content_type,
                              const RequestOptions& opts) const {
    auto parsed = parse_url(url);
    auto& entry = get_client(parsed.scheme, parsed.host, parsed.port);
    std::lock_guard<std::mutex> lock(entry.mutex);

    auto headers = merged_headers(opts);
    auto result = entry.client->Put(
        parsed.path, to_httplib_headers(headers), body, content_type);
    return make_response(result);
}

// ── PATCH ────────────────────────────────────────────────────────

HttpResponse HttpClient::patch(const std::string& url,
                                const std::string& body,
                                const std::string& content_type,
                                const RequestOptions& opts) const {
    auto parsed = parse_url(url);
    auto& entry = get_client(parsed.scheme, parsed.host, parsed.port);
    std::lock_guard<std::mutex> lock(entry.mutex);

    auto headers = merged_headers(opts);
    auto result = entry.client->Patch(
        parsed.path, to_httplib_headers(headers), body, content_type);
    return make_response(result);
}

// ── DELETE ───────────────────────────────────────────────────────

HttpResponse HttpClient::delete_(const std::string& url,
                                  const RequestOptions& opts) const {
    auto parsed = parse_url(url);
    auto& entry = get_client(parsed.scheme, parsed.host, parsed.port);
    std::lock_guard<std::mutex> lock(entry.mutex);

    auto headers = merged_headers(opts);
    auto result = entry.client->Delete(
        parsed.path, to_httplib_headers(headers));
    return make_response(result);
}

// ── POST Form ────────────────────────────────────────────────────

HttpResponse HttpClient::post_form(
    const std::string& url,
    const std::map<std::string, std::string>& form_data,
    const RequestOptions& opts) const {

    auto parsed = parse_url(url);
    auto& entry = get_client(parsed.scheme, parsed.host, parsed.port);
    std::lock_guard<std::mutex> lock(entry.mutex);

    auto headers = merged_headers(opts);

    httplib::Params params;
    for (const auto& [k, v] : form_data) {
        params.emplace(k, v);
    }

    auto result = entry.client->Post(
        parsed.path, to_httplib_headers(headers), params);
    return make_response(result);
}

// ── JSON convenience ─────────────────────────────────────────────

std::pair<nlohmann::json, HttpResponse> HttpClient::get_json(
    const std::string& url,
    const RequestOptions& opts) const {

    auto resp = get(url, opts);
    return {resp.json(), resp};
}

// ── Factory functions ────────────────────────────────────────────

HttpClient create_client() {
    return HttpClient();
}

HttpClient create_client_with_bearer(const std::string& token) {
    HttpClient client;
    client.with_bearer_token(token);
    return client;
}

HttpClient create_client_with_api_key(const std::string& header_name,
                                       const std::string& key) {
    HttpClient client;
    client.with_api_key(header_name, key);
    return client;
}

} // namespace http
} // namespace zeroclaw
