#pragma once

#include "gateway.hpp"
#include <string>
#include <optional>
#include <vector>

namespace zeroclaw::gateway::api {

struct MemoryQuery {
    std::optional<std::string> query;
    std::optional<std::string> category;
};

struct MemoryStoreBody {
    std::string key;
    std::string content;
    std::optional<std::string> category;
};

struct CronAddBody {
    std::optional<std::string> name;
    std::string schedule;
    std::string command;
};

struct WhatsAppVerifyQuery {
    std::optional<std::string> mode;
    std::optional<std::string> verify_token;
    std::optional<std::string> challenge;
};

struct WebhookBody {
    std::string message;
};

std::optional<std::string> extract_bearer_token(const http::HeaderMap& headers);

std::optional<http::Response> require_auth(const AppState& state, const http::HeaderMap& headers);

http::Response handle_api_status(AppState& state, const http::Request& req);
http::Response handle_api_config_get(AppState& state, const http::Request& req);
http::Response handle_api_config_put(AppState& state, const http::Request& req);
http::Response handle_api_tools(AppState& state, const http::Request& req);
http::Response handle_api_cron_list(AppState& state, const http::Request& req);
http::Response handle_api_cron_add(AppState& state, const http::Request& req);
http::Response handle_api_cron_delete(AppState& state, const http::Request& req);
http::Response handle_api_integrations(AppState& state, const http::Request& req);
http::Response handle_api_doctor(AppState& state, const http::Request& req);
http::Response handle_api_memory_list(AppState& state, const http::Request& req);
http::Response handle_api_memory_store(AppState& state, const http::Request& req);
http::Response handle_api_memory_delete(AppState& state, const http::Request& req);
http::Response handle_api_cost(AppState& state, const http::Request& req);
http::Response handle_api_cli_tools(AppState& state, const http::Request& req);
http::Response handle_api_health(AppState& state, const http::Request& req);

std::string mask_sensitive_fields(const std::string& toml_str);

}
