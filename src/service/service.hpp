#pragma once

/// Service module — HTTP server for webhook-based channels (WhatsApp, Slack, etc.)

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace service {

/// HTTP request received by the service
struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::pair<std::string, std::string>> query_params;

    std::optional<std::string> header(const std::string& name) const;
    std::optional<std::string> query(const std::string& name) const;
};

/// HTTP response to send back
struct HttpResponse {
    int status_code = 200;
    std::string body;
    std::string content_type = "application/json";
    std::vector<std::pair<std::string, std::string>> headers;

    static HttpResponse ok(const std::string& body = "");
    static HttpResponse json(const nlohmann::json& j);
    static HttpResponse error(int code, const std::string& message);
};

/// Route handler type
using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

/// Simple HTTP server for webhook handling
class HttpServer {
public:
    explicit HttpServer(uint16_t port = 8080);

    /// Register a route handler
    void route(const std::string& method, const std::string& path, RouteHandler handler);

    /// Start listening (blocking)
    void start();

    /// Stop the server
    void stop();

    uint16_t port() const { return port_; }

private:
    uint16_t port_;
    bool running_ = false;
    std::vector<std::tuple<std::string, std::string, RouteHandler>> routes_;
};

} // namespace service
} // namespace zeroclaw
