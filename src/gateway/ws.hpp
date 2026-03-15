#pragma once

#include "gateway.hpp"
#include <functional>
#include <string>
#include <optional>
#include <vector>

namespace zeroclaw::gateway::ws {

struct WsQuery {
    std::optional<std::string> token;
};

enum class MessageType {
    Text,
    Binary,
    Close,
    Ping,
    Pong
};

struct WebSocketMessage {
    MessageType type;
    std::vector<uint8_t> data;
    
    static WebSocketMessage text(const std::string& content);
    static WebSocketMessage binary(const std::vector<uint8_t>& data);
    static WebSocketMessage close();
};

class IWebSocket {
public:
    virtual ~IWebSocket() = default;
    virtual void send(const WebSocketMessage& msg) = 0;
    virtual std::optional<WebSocketMessage> receive() = 0;
    virtual bool is_open() const = 0;
    virtual void close() = 0;
};

using WebSocketHandler = std::function<void(IWebSocket&, AppState&)>;

http::Response handle_ws_chat(AppState& state, const http::Request& req);

void set_websocket_handler(WebSocketHandler handler);

struct WsIncomingMessage {
    std::string type;
    std::string content;
    
    static std::optional<WsIncomingMessage> parse(const std::string& json);
};

struct WsOutgoingMessage {
    std::string type;
    std::string content;
    std::optional<std::string> full_response;
    std::optional<std::string> name;
    std::optional<std::string> message;
    
    std::string to_json() const;
};

}
