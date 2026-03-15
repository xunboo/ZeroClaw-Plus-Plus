#include "nextcloud_talk.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>

namespace zeroclaw {
namespace channels {

// ── Helpers ──────────────────────────────────────────────────────

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

// ── NextcloudTalkChannel ─────────────────────────────────────────

NextcloudTalkChannel::NextcloudTalkChannel(const std::string& base_url,
                                           const std::string& app_token,
                                           const std::vector<std::string>& allowed_users)
    : base_url_(base_url), app_token_(app_token), allowed_users_(allowed_users) {
    // Trim trailing slash from base_url
    while (!base_url_.empty() && base_url_.back() == '/') {
        base_url_.pop_back();
    }
}

bool NextcloudTalkChannel::is_user_allowed(const std::string& actor_id) const {
    for (const auto& u : allowed_users_) {
        if (u == "*" || u == actor_id) return true;
    }
    return false;
}

uint64_t NextcloudTalkChannel::parse_timestamp_secs(const nlohmann::json& value) {
    uint64_t raw = 0;
    if (value.is_number_unsigned()) {
        raw = value.get<uint64_t>();
    } else if (value.is_string()) {
        try { raw = std::stoull(value.get<std::string>()); }
        catch (...) { raw = 0; }
    }
    if (raw == 0) {
        // fallback to current time
        raw = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
    // Some payloads use milliseconds
    if (raw > 1'000'000'000'000ULL) {
        raw /= 1000;
    }
    return raw;
}

std::optional<std::string> NextcloudTalkChannel::value_to_string(const nlohmann::json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number()) return std::to_string(value.get<int64_t>());
    return std::nullopt;
}

// ── parse_webhook_payload ─────────────────────────────────────────

std::vector<ChannelMessage> NextcloudTalkChannel::parse_webhook_payload(
    const nlohmann::json& payload) const {

    std::vector<ChannelMessage> messages;

    // Accept "message" or "Create" event types (case-insensitive)
    if (payload.contains("type") && payload["type"].is_string()) {
        std::string event_type = to_lower(payload["type"].get<std::string>());
        bool is_message_event = (event_type == "message" || event_type == "create");
        if (!is_message_event) {
            // Not a message event, skip
            return messages;
        }
    }

    if (!payload.contains("message")) {
        return messages;
    }
    const auto& message_obj = payload["message"];

    // Extract room token from object.token or message.token
    std::string room_token;
    if (payload.contains("object") && payload["object"].contains("token") &&
        payload["object"]["token"].is_string()) {
        room_token = trim(payload["object"]["token"].get<std::string>());
    } else if (message_obj.contains("token") && message_obj["token"].is_string()) {
        room_token = trim(message_obj["token"].get<std::string>());
    }
    if (room_token.empty()) {
        return messages;
    }

    // Check actor type — ignore bot-originated messages
    std::string actor_type;
    if (message_obj.contains("actorType") && message_obj["actorType"].is_string()) {
        actor_type = message_obj["actorType"].get<std::string>();
    } else if (payload.contains("actorType") && payload["actorType"].is_string()) {
        actor_type = payload["actorType"].get<std::string>();
    }
    if (to_lower(actor_type) == "bots") {
        return messages;  // skip bot-originated messages
    }

    // Extract actor ID
    std::string actor_id;
    if (message_obj.contains("actorId") && message_obj["actorId"].is_string()) {
        actor_id = trim(message_obj["actorId"].get<std::string>());
    } else if (payload.contains("actorId") && payload["actorId"].is_string()) {
        actor_id = trim(payload["actorId"].get<std::string>());
    }
    if (actor_id.empty()) {
        return messages;
    }

    // Check allowlist
    if (!is_user_allowed(actor_id)) {
        return messages;
    }

    // Only accept "comment" messageType
    std::string message_type = "comment";
    if (message_obj.contains("messageType") && message_obj["messageType"].is_string()) {
        message_type = message_obj["messageType"].get<std::string>();
    }
    if (to_lower(message_type) != "comment") {
        return messages;
    }

    // Skip system messages (non-empty systemMessage field)
    if (message_obj.contains("systemMessage") && message_obj["systemMessage"].is_string()) {
        if (!trim(message_obj["systemMessage"].get<std::string>()).empty()) {
            return messages;
        }
    }

    // Extract message text
    std::string content;
    if (message_obj.contains("message") && message_obj["message"].is_string()) {
        content = trim(message_obj["message"].get<std::string>());
    }
    if (content.empty()) {
        return messages;
    }

    // Extract timestamp
    uint64_t timestamp = 0;
    if (message_obj.contains("timestamp")) {
        timestamp = parse_timestamp_secs(message_obj["timestamp"]);
    }

    // Extract message ID
    std::string msg_id;
    if (message_obj.contains("id")) {
        auto opt = value_to_string(message_obj["id"]);
        if (opt.has_value()) msg_id = *opt;
    }

    ChannelMessage cm;
    cm.id = msg_id.empty() ? room_token + "_" + std::to_string(timestamp) : msg_id;
    cm.sender = actor_id;
    cm.reply_target = room_token;
    cm.content = content;
    cm.channel = "nextcloud_talk";
    cm.timestamp = timestamp;
    messages.push_back(std::move(cm));
    return messages;
}

// ── send ──────────────────────────────────────────────────────────

bool NextcloudTalkChannel::send(const SendMessage& message) {
    // POST to Nextcloud Talk OCS API: /ocs/v2.php/apps/spreed/api/v1/chat/{token}
    std::string url = base_url_ + "/ocs/v2.php/apps/spreed/api/v1/chat/" + message.recipient;

    nlohmann::json body = {{"message", message.content}};

    http::HttpClient client;
    client.with_api_key("Authorization", "Basic " + app_token_);
    client.with_header("OCS-APIRequest", "true");
    auto resp = client.post_json(url, body);
    return resp.ok();
}

// ── listen ────────────────────────────────────────────────────────

bool NextcloudTalkChannel::listen(std::function<void(const ChannelMessage&)> /*callback*/) {
    // Nextcloud Talk uses webhook mode; messages arrive via /nextcloud-talk gateway endpoint
    // and are dispatched via parse_webhook_payload().
    return true;
}

// ── health_check ──────────────────────────────────────────────────

bool NextcloudTalkChannel::health_check() const {
    return !base_url_.empty() && !app_token_.empty();
}

// ── verify_signature ─────────────────────────────────────────────

bool NextcloudTalkChannel::verify_signature(const std::string& /*body*/,
                                              const std::string& /*signature*/) const {
    // Full implementation would compute HMAC-SHA256 of body using app_token_ as key
    // and compare with the provided signature in constant time.
    // Requires an HMAC library (e.g., OpenSSL or Botan).
    return true;  // stub — always accept during development
}

} // namespace channels
} // namespace zeroclaw
