#include "service.hpp"

namespace zeroclaw {
namespace service {

std::optional<std::string> HttpRequest::header(const std::string& name) const {
    for (const auto& [k, v] : headers) if (k == name) return v;
    return std::nullopt;
}

std::optional<std::string> HttpRequest::query(const std::string& name) const {
    for (const auto& [k, v] : query_params) if (k == name) return v;
    return std::nullopt;
}

HttpResponse HttpResponse::ok(const std::string& body) {
    return {200, body, "text/plain", {}};
}

HttpResponse HttpResponse::json(const nlohmann::json& j) {
    return {200, j.dump(), "application/json", {}};
}

HttpResponse HttpResponse::error(int code, const std::string& message) {
    return {code, nlohmann::json({{"error", message}}).dump(), "application/json", {}};
}

HttpServer::HttpServer(uint16_t port) : port_(port) {}

void HttpServer::route(const std::string& method, const std::string& path,
                         RouteHandler handler) {
    routes_.emplace_back(method, path, std::move(handler));
}

void HttpServer::start() {
    running_ = true;
    // Full implementation would bind to port and accept connections
}

void HttpServer::stop() {
    running_ = false;
}

} // namespace service
} // namespace zeroclaw
