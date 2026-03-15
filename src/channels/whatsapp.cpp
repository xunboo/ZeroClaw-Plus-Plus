#include "whatsapp.hpp"
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

                auto now = std::chrono::system_clock::now();
                uint64_t ts = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::seconds>(
                        now.time_since_epoch()).count());

                ChannelMessage cm;
                cm.id = msg.contains("id") ? msg["id"].get<std::string>() : "";
                cm.sender = from;
                cm.reply_target = from;
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
    // Would POST to graph.facebook.com /v18.0/{endpoint_id}/messages
    (void)message;
    return true;
}

bool WhatsAppChannel::listen(std::function<void(const ChannelMessage&)> /*callback*/) {
    // Webhook-based ?messages received via gateway endpoint

    return true;
}

bool WhatsAppChannel::health_check() const {
    return !access_token_.empty() && !endpoint_id_.empty();
}

} // namespace channels
} // namespace zeroclaw


