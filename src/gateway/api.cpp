#include "api.hpp"
#include <sstream>
#include "config/config.hpp"
#include "security/pairing.hpp"
#include "tools/traits.hpp"

namespace zeroclaw::gateway::api {

namespace {
    bool str_starts_with(const std::string& s, const std::string& prefix) {
        return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
    }
}

std::optional<std::string> extract_bearer_token(const http::HeaderMap& headers) {
    auto auth = headers.get("Authorization");
    if (!auth) {
        return std::nullopt;
    }
    
    const std::string prefix = "Bearer ";
    if (str_starts_with(*auth, prefix)) {
        return auth->substr(prefix.size());
    }
    
    return std::nullopt;
}

std::optional<http::Response> require_auth(const AppState& state, const http::HeaderMap& headers) {
    if (!state.pairing || !state.pairing->require_pairing()) {
        return std::nullopt;
    }
    
    auto token = extract_bearer_token(headers).value_or("");
    if (state.pairing->is_authenticated(token)) {
        return std::nullopt;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::UNAUTHORIZED;
    resp.body = R"({"error":"Unauthorized — pair first via POST /pair, then send Authorization: Bearer <token>"})";
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_status(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    std::ostringstream oss;
    oss << R"({"provider":")" << (state.config->default_provider.value_or("unknown")) << R"(",)";
    oss << R"("model":")" << state.model << R"(",)";
    oss << R"("temperature":)" << state.temperature << ",";
    oss << R"("paired":)" << (state.pairing && state.pairing->is_paired() ? "true" : "false");
    oss << "}";
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = oss.str();
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_config_get(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    std::string toml_content = state.config->to_toml();
    std::string masked = mask_sensitive_fields(toml_content);
    
    std::ostringstream oss;
    oss << R"({"format":"toml","content":")";
    for (char c : masked) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default: oss << c;
        }
    }
    oss << "\"}";
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = oss.str();
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_config_put(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    auto new_config = config::Config::from_toml(req.body);
    if (!new_config) {
        http::Response resp;
        resp.status_code = http::StatusCode::BAD_REQUEST;
        resp.body = R"({"error":"Invalid TOML"})";
        resp.content_type = "application/json";
        return resp;
    }
    
    if (!new_config->save()) {
        http::Response resp;
        resp.status_code = http::StatusCode::INTERNAL_SERVER_ERROR;
        resp.body = R"({"error":"Failed to save config"})";
        resp.content_type = "application/json";
        return resp;
    }
    
    state.config = std::make_shared<config::Config>(std::move(*new_config));
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = R"({"status":"ok"})";
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_tools(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    std::ostringstream oss;
    oss << R"({"tools":[)";
    
    if (state.tools_registry) {
        bool first = true;
        for (const auto& tool : *state.tools_registry) {
            if (!first) oss << ",";
            oss << R"({"name":")" << tool.name << R"(",)";
            oss << R"("description":")" << tool.description << R"("})";
            first = false;
        }
    }
    
    oss << "]}";
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = oss.str();
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_cron_list(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = R"({"jobs":[]})";
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_cron_add(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = R"({"status":"ok"})";
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_cron_delete(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = R"({"status":"ok"})";
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_integrations(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = R"({"integrations":[]})";
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_doctor(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = R"({"results":[],"summary":{"ok":0,"warnings":0,"errors":0}})";
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_memory_list(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = R"({"entries":[]})";
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_memory_store(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = R"({"status":"ok"})";
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_memory_delete(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = R"({"status":"ok","deleted":true})";
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_cost(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = R"({"cost":{"session_cost_usd":0.0,"daily_cost_usd":0.0,"monthly_cost_usd":0.0,"total_tokens":0,"request_count":0,"by_model":{}}})";
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_cli_tools(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = R"({"cli_tools":[]})";
    resp.content_type = "application/json";
    return resp;
}

http::Response handle_api_health(AppState& state, const http::Request& req) {
    if (auto err = require_auth(state, req.headers)) {
        return *err;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.body = R"({"health":{"status":"ok"}})";
    resp.content_type = "application/json";
    return resp;
}

std::string mask_sensitive_fields(const std::string& toml_str) {
    std::ostringstream output;
    std::istringstream input(toml_str);
    std::string line;
    
    while (std::getline(input, line)) {
        auto trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t");
        if (start != std::string::npos) {
            trimmed = trimmed.substr(start);
        }
        
        bool should_mask = str_starts_with(trimmed, "api_key")
                        || str_starts_with(trimmed, "bot_token")
                        || str_starts_with(trimmed, "access_token")
                        || str_starts_with(trimmed, "secret")
                        || str_starts_with(trimmed, "app_secret")
                        || str_starts_with(trimmed, "signing_secret");
        
        if (should_mask) {
            auto eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                output << line.substr(0, eq_pos + 1) << " \"***MASKED***\"\n";
                continue;
            }
        }
        
        output << line << "\n";
    }
    
    return output.str();
}

}
