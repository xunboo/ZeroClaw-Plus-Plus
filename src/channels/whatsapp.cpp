#include "whatsapp.hpp"
#include "../http/http_client.hpp"
#include <chrono>

namespace zeroclaw {
namespace channels {

WhatsAppChannel::WhatsAppChannel(const std::string& access_token,
                                  const std::string& endpoint_id,
                                  const std::string& verify_token,
                                  const std::vector<std::string>& allowed_numbers)
    : access_token_(access_token), endpoint_id_(endpoint_id),
      verify_token_(verify_token), allowed_numbers_(allowed_numbers) {}

bool WhatsAppChannel::is_number_allowed(const std::string& phone) const {
    if (allowed_numbers_.empty()) return false;
    // Normalize phone: ensure + prefix
    std::string normalized = phone;
    if (!normalized.empty() && normalized[0] != '+') {
        normalized = "+" + normalized;
    }
    for (const auto& allowed : allowed_numbers_) {
        if (allowed == "*") return true;
        std::string norm_allowed = allowed;
        if (!norm_allowed.empty() && norm_allowed[0] != '+') {
            norm_allowed = "+" + norm_allowed;
        }
        if (normalized == norm_allowed) return true;
    }
    return false;
}

std::vector<ChannelMessage>
WhatsAppChannel::parse_webhook_payload(const nlohmann::json& payload) const {
    std::vector<ChannelMessage> messages;

    if (!payload.contains("entry") || !payload["entry"].is_array()) return messages;

    for (const auto& entry : payload["entry"]) {
        if (!entry.contains("changes") || !entry["changes"].is_array()) continue;

        for (const auto& change : entry["changes"]) {
            if (!change.contains("value") || !change["value"].is_object()) continue;
            const auto& value = change["value"];
            if (!value.contains("messages") || !value["messages"].is_array()) continue;

            for (const auto& msg : value["messages"]) {
                if (!msg.contains("type") || msg["type"] != "text") continue;
                if (!msg.contains("from") || !msg["from"].is_string()) continue;
                if (!msg.contains("text") || !msg["text"].is_object()) continue;

                std::string from = msg["from"].get<std::string>();
                if (!is_number_allowed(from)) continue;

                std::string body;
                if (msg["text"].contains("body") && msg["text"]["body"].is_string()) {
                    body = msg["text"]["body"].get<std::string>();
                }
                if (body.empty()) continue;

                // Rust: try to parse msg["timestamp"] as a u64 string,
                // fall back to now() only if missing or unparseable.
                uint64_t ts = 0;
                bool parsed_ts = false;
                if (msg.contains("timestamp")) {
                    if (msg["timestamp"].is_string()) {
                        try {
                            ts = std::stoull(msg["timestamp"].get<std::string>());
                            parsed_ts = true;
                        } catch (...) {}
                    } else if (msg["timestamp"].is_number_unsigned()) {
                        ts = msg["timestamp"].get<uint64_t>();
                        parsed_ts = true;
                    }
                }
                if (!parsed_ts) {
                    auto now = std::chrono::system_clock::now();
                    ts = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::seconds>(
                            now.time_since_epoch()).count());
                }

                // Normalize sender: ensure + prefix (matching Rust normalize logic)
                std::string sender = from;
                if (!sender.empty() && sender[0] != '+') {
                    sender = "+" + sender;
                }

                ChannelMessage cm;
                cm.id = msg.contains("id") ? msg["id"].get<std::string>() : "";
                cm.sender = sender;
                cm.reply_target = sender;
                cm.content = body;
                cm.channel = "whatsapp";
                cm.timestamp = ts;
                messages.push_back(cm);
            }
        }
    }

    return messages;
}

bool WhatsAppChannel::send(const SendMessage& message) {
    // Matching Rust: POST https://graph.facebook.com/v18.0/{endpoint_id}/messages
    // with Authorization: Bearer {access_token}
    // Body: { "messaging_product": "whatsapp", "to": "<recipient without +>",
    //         "type": "text", "text": { "body": "<content>" } }
    //
    // Rust strips the leading '+' from the recipient for the API call.
    std::string to = message.recipient;
    if (!to.empty() && to[0] == '+') {
        to = to.substr(1);
    }

    nlohmann::json body = {
        {"messaging_product", "whatsapp"},
        {"to", to},
        {"type", "text"},
        {"text", {{"body", message.content}}}
    };

    http::HttpClient client;
    client.with_bearer_token(access_token_);

    const std::string url =
        "https://graph.facebook.com/v18.0/" + endpoint_id_ + "/messages";
    auto resp = client.post_json(url, body);
    return resp.ok();
}

bool WhatsAppChannel::listen(std::function<void(const ChannelMessage&)> /*callback*/) {
    // WhatsApp is webhook-based; messages arrive via the gateway endpoint.
    // Matches Rust's placeholder loop that simply awaits cancellation.
    return true;
}

bool WhatsAppChannel::health_check() const {
    return !access_token_.empty() && !endpoint_id_.empty();
}

} // namespace channels
} // namespace zeroclaw
