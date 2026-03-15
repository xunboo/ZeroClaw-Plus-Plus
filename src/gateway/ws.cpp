#include "ws.hpp"
#include <mutex>
#include <sstream>
#include "security/pairing.hpp"

namespace zeroclaw::gateway::ws {

namespace {
    WebSocketHandler g_ws_handler;
    std::mutex g_handler_mutex;
}

WebSocketMessage WebSocketMessage::text(const std::string& content) {
    WebSocketMessage msg;
    msg.type = MessageType::Text;
    msg.data.assign(content.begin(), content.end());
    return msg;
}

WebSocketMessage WebSocketMessage::binary(const std::vector<uint8_t>& data) {
    WebSocketMessage msg;
    msg.type = MessageType::Binary;
    msg.data = data;
    return msg;
}

WebSocketMessage WebSocketMessage::close() {
    WebSocketMessage msg;
    msg.type = MessageType::Close;
    return msg;
}

void set_websocket_handler(WebSocketHandler handler) {
    std::lock_guard<std::mutex> lock(g_handler_mutex);
    g_ws_handler = std::move(handler);
}

http::Response handle_ws_chat(AppState& state, const http::Request& req) {
    if (state.pairing && state.pairing->require_pairing()) {
        auto query = req.path.find('?');
        std::string token;
        
        if (query != std::string::npos) {
            auto params = req.path.substr(query + 1);
            auto token_pos = params.find("token=");
            if (token_pos != std::string::npos) {
                auto start = token_pos + 6;
                auto end = params.find('&', start);
                token = params.substr(start, end - start);
            }
        }
        
        if (!state.pairing->is_authenticated(token)) {
            http::Response resp;
            resp.status_code = http::StatusCode::UNAUTHORIZED;
            resp.body = "Unauthorized — provide ?token=<bearer_token>";
            resp.content_type = "text/plain";
            return resp;
        }
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.headers.set("Upgrade", "websocket");
    resp.headers.set("Connection", "Upgrade");
    resp.content_type = "";
    resp.body = "";
    return resp;
}

std::optional<WsIncomingMessage> WsIncomingMessage::parse(const std::string& json) {
    WsIncomingMessage msg;
    
    auto type_pos = json.find(R"("type")");
    if (type_pos == std::string::npos) {
        return std::nullopt;
    }
    
    auto type_start = json.find(':', type_pos);
    if (type_start == std::string::npos) {
        return std::nullopt;
    }
    
    auto quote_start = json.find('"', type_start);
    if (quote_start == std::string::npos) {
        return std::nullopt;
    }
    
    auto quote_end = json.find('"', quote_start + 1);
    if (quote_end == std::string::npos) {
        return std::nullopt;
    }
    
    msg.type = json.substr(quote_start + 1, quote_end - quote_start - 1);
    
    auto content_pos = json.find(R"("content")");
    if (content_pos != std::string::npos) {
        auto content_start = json.find(':', content_pos);
        if (content_start != std::string::npos) {
            auto content_quote_start = json.find('"', content_start);
            if (content_quote_start != std::string::npos) {
                auto content_quote_end = json.find('"', content_quote_start + 1);
                if (content_quote_end != std::string::npos) {
                    msg.content = json.substr(content_quote_start + 1, content_quote_end - content_quote_start - 1);
                }
            }
        }
    }
    
    return msg;
}

std::string WsOutgoingMessage::to_json() const {
    std::ostringstream oss;
    oss << R"({"type":")" << type << R"(")";
    
    if (!content.empty()) {
        oss << R"(,"content":")" << content << R"(")";
    }
    
    if (full_response.has_value()) {
        oss << R"(,"full_response":")" << *full_response << R"(")";
    }
    
    if (name.has_value()) {
        oss << R"(,"name":")" << *name << R"(")";
    }
    
    if (message.has_value()) {
        oss << R"(,"message":")" << *message << R"(")";
    }
    
    oss << "}";
    return oss.str();
}

}
