#include "sse.hpp"
#include <sstream>
#include <chrono>
#include <variant>
#include "security/pairing.hpp"
#include "gateway/api.hpp"

namespace zeroclaw::gateway::sse {

namespace observability = zeroclaw::observability;

http::Response handle_sse_events(AppState& state, const http::Request& req) {
    if (state.pairing && state.pairing->require_pairing()) {
        auto token = api::extract_bearer_token(req.headers).value_or("");
        if (!state.pairing->is_authenticated(token)) {
            http::Response resp;
            resp.status_code = http::StatusCode::UNAUTHORIZED;
            resp.body = "Unauthorized — provide Authorization: Bearer <token>";
            resp.content_type = "text/plain";
            return resp;
        }
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.content_type = "text/event-stream";
    resp.headers.set("Cache-Control", "no-cache");
    resp.headers.set("Connection", "keep-alive");
    resp.body = "";
    return resp;
}

BroadcastObserver::BroadcastObserver(std::unique_ptr<observability::Observer> inner,
                                     std::function<void(const std::string&)> broadcaster)
    : inner_(std::move(inner))
    , broadcaster_(std::move(broadcaster)) {}

void BroadcastObserver::record_event(const observability::ObserverEvent& event) {
    if (inner_) {
        inner_->record_event(event);
    }
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    
    std::ostringstream oss;
    oss << R"({"timestamp":")" << timestamp << R"(",)";
    
    std::visit([&oss](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, observability::LlmRequestEvent>) {
            oss << R"("type":"llm_request",)";
            oss << R"("provider":")" << arg.provider << R"(",)";
            oss << R"("model":")" << arg.model << R"(")";
        } else if constexpr (std::is_same_v<T, observability::ToolCallEvent>) {
            oss << R"("type":"tool_call",)";
            oss << R"("tool":")" << arg.tool << R"(",)";
            oss << R"("duration_ms":)" << arg.duration_ms << ",";
            oss << R"("success":)" << (arg.success ? "true" : "false");
        } else if constexpr (std::is_same_v<T, observability::ToolCallStartEvent>) {
            oss << R"("type":"tool_call_start",)";
            oss << R"("tool":")" << arg.tool << R"(")";
        } else if constexpr (std::is_same_v<T, observability::ErrorEvent>) {
            oss << R"("type":"error",)";
            oss << R"("component":")" << arg.component << R"(",)";
            oss << R"("message":")" << arg.message << R"(")";
        } else if constexpr (std::is_same_v<T, observability::AgentStartEvent>) {
            oss << R"("type":"agent_start",)";
            oss << R"("provider":")" << arg.provider << R"(",)";
            oss << R"("model":")" << arg.model << R"(")";
        } else if constexpr (std::is_same_v<T, observability::AgentEndEvent>) {
            oss << R"("type":"agent_end",)";
            oss << R"("provider":")" << arg.provider << R"(",)";
            oss << R"("model":")" << arg.model << R"(",)";
            oss << R"("duration_ms":)" << arg.duration_ms << ",";
            oss << R"("tokens_used":)" << (arg.tokens_used.has_value() ? std::to_string(*arg.tokens_used) : "null");
        }
    }, event);
    
    oss << "}";
    
    if (broadcaster_) {
        broadcaster_(oss.str());
    }
}

void BroadcastObserver::record_metric(const observability::ObserverMetric& metric) {
    if (inner_) {
        inner_->record_metric(metric);
    }
}

void BroadcastObserver::flush() {
    if (inner_) {
        inner_->flush();
    }
}

std::string BroadcastObserver::name() const {
    return "broadcast";
}

}
