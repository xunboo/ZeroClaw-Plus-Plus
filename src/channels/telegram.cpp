#include "telegram.hpp"
#include "../http/http_client.hpp"
#include <algorithm>

namespace zeroclaw {
namespace channels {

// ── Message splitting ────────────────────────────────────────────

std::vector<std::string> split_message_for_telegram(const std::string& message) {
    const size_t max_len = TELEGRAM_MAX_MESSAGE_LENGTH - TELEGRAM_CONTINUATION_OVERHEAD;
    std::vector<std::string> chunks;

    if (message.size() <= TELEGRAM_MAX_MESSAGE_LENGTH) {
        chunks.push_back(message);
        return chunks;
    }

    size_t pos = 0;
    while (pos < message.size()) {
        size_t remaining = message.size() - pos;
        if (remaining <= max_len) {
            chunks.push_back(message.substr(pos));
            break;
        }

        // Find word boundary
        size_t split_at = pos + max_len;
        size_t word_break = message.rfind(' ', split_at);
        if (word_break != std::string::npos && word_break > pos) {
            split_at = word_break;
        }

        chunks.push_back(message.substr(pos, split_at - pos));
        pos = split_at;
        if (pos < message.size() && message[pos] == ' ') ++pos;
    }

    return chunks;
}

std::pair<std::string, std::vector<TelegramAttachment>>
parse_attachment_markers(const std::string& message) {
    std::string clean_text;
    std::vector<TelegramAttachment> attachments;

    const std::vector<std::pair<std::string, TelegramAttachmentKind>> markers = {
        {"[IMAGE:", TelegramAttachmentKind::Image},
        {"[DOCUMENT:", TelegramAttachmentKind::Document},
        {"[VIDEO:", TelegramAttachmentKind::Video},
        {"[AUDIO:", TelegramAttachmentKind::Audio},
        {"[VOICE:", TelegramAttachmentKind::Voice},
    };

    size_t pos = 0;
    while (pos < message.size()) {
        bool found = false;
        for (const auto& [marker, kind] : markers) {
            if (message.compare(pos, marker.size(), marker) == 0) {
                auto end_pos = message.find(']', pos + marker.size());
                if (end_pos != std::string::npos) {
                    std::string target = message.substr(
                        pos + marker.size(), end_pos - pos - marker.size());
                    attachments.push_back({kind, target});
                    pos = end_pos + 1;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            clean_text += message[pos];
            ++pos;
        }
    }

    return {clean_text, attachments};
}

// ── TelegramChannel ──────────────────────────────────────────────

TelegramChannel::TelegramChannel(const std::string& bot_token,
                                  const std::vector<std::string>& allowed_users,
                                  bool mention_only)
    : bot_token_(bot_token), allowed_users_(allowed_users), mention_only_(mention_only) {}

TelegramChannel& TelegramChannel::with_workspace_dir(const std::string& dir) {
    workspace_dir_ = dir;
    return *this;
}

TelegramChannel& TelegramChannel::with_streaming(bool enabled, uint64_t draft_interval_ms) {
    streaming_ = enabled;
    draft_interval_ms_ = draft_interval_ms;
    return *this;
}

TelegramChannel& TelegramChannel::with_api_base(const std::string& api_base) {
    api_base_ = api_base;
    return *this;
}

bool TelegramChannel::is_user_allowed(const std::string& user_id) const {
    if (allowed_users_.empty()) return false;
    for (const auto& allowed : allowed_users_) {
        if (allowed == "*" || allowed == user_id) return true;
    }
    return false;
}

std::pair<std::string, std::optional<std::string>>
TelegramChannel::parse_reply_target(const std::string& reply_target) const {
    auto sep = reply_target.find(':');
    if (sep != std::string::npos) {
        return {reply_target.substr(0, sep), reply_target.substr(sep + 1)};
    }
    return {reply_target, std::nullopt};
}

// ── Helper: build Bot API URL ────────────────────────────────────

static std::string bot_api_url(const std::string& api_base,
                                const std::string& bot_token,
                                const std::string& method) {
    return api_base + "/bot" + bot_token + "/" + method;
}

// ── send ─────────────────────────────────────────────────────────

bool TelegramChannel::send(const SendMessage& message) {
    auto [clean_text, attachments] = parse_attachment_markers(message.content);
    auto [chat_id, thread_id] = parse_reply_target(message.recipient);

    // Send attachments first
    for (const auto& attachment : attachments) {
        send_attachment(chat_id, thread_id, attachment);
    }

    if (clean_text.empty()) return true;

    http::HttpClient client;
    auto chunks = split_message_for_telegram(clean_text);

    for (const auto& chunk : chunks) {
        nlohmann::json body = {
            {"chat_id", chat_id},
            {"text", chunk},
            {"parse_mode", "Markdown"}
        };
        if (thread_id.has_value()) {
            body["message_thread_id"] = *thread_id;
        }

        auto url = bot_api_url(api_base_, bot_token_, "sendMessage");
        auto resp = client.post_json(url, body);
        if (!resp.ok()) return false;
    }
    return true;
}

// ── listen ───────────────────────────────────────────────────────

bool TelegramChannel::listen(std::function<void(const ChannelMessage&)> callback) {
    http::HttpClient client;
    client.with_timeout(60);  // Long-poll timeout

    int64_t offset = 0;
    while (true) {
        nlohmann::json params = {
            {"timeout", 30},
            {"allowed_updates", nlohmann::json::array({"message"})}
        };
        if (offset > 0) {
            params["offset"] = offset;
        }

        auto url = bot_api_url(api_base_, bot_token_, "getUpdates");
        auto resp = client.post_json(url, params);

        if (!resp.ok()) {
            // Backoff on error
            continue;
        }

        auto json_resp = resp.json();
        if (!json_resp.contains("result") || !json_resp["result"].is_array()) {
            continue;
        }

        for (const auto& update : json_resp["result"]) {
            int64_t update_id = update.value("update_id", int64_t(0));
            offset = update_id + 1;

            if (!update.contains("message")) continue;
            auto& msg = update["message"];

            // Extract sender info
            std::string user_id;
            std::string user_name;
            if (msg.contains("from")) {
                user_id = std::to_string(msg["from"].value("id", int64_t(0)));
                user_name = msg["from"].value("username", "");
                if (user_name.empty()) {
                    user_name = msg["from"].value("first_name", "");
                }
            }

            // Check user allowlist
            if (!is_user_allowed(user_id) && !is_user_allowed(user_name)) {
                continue;
            }

            // Build ChannelMessage
            ChannelMessage channel_msg;
            channel_msg.content = msg.value("text", "");
            channel_msg.sender = user_name.empty() ? user_id : user_name;

            std::string chat_id_str = std::to_string(msg["chat"].value("id", int64_t(0)));
            channel_msg.reply_target = chat_id_str;

            // Include thread_id if present
            if (msg.contains("message_thread_id")) {
                channel_msg.reply_target += ":" +
                    std::to_string(msg["message_thread_id"].get<int64_t>());
            }

            channel_msg.channel = "telegram";
            channel_msg.id = std::to_string(msg.value("message_id", int64_t(0)));

            if (!channel_msg.content.empty()) {
                callback(channel_msg);
            }
        }
    }
}

// ── health_check ─────────────────────────────────────────────────

bool TelegramChannel::health_check() const {
    if (bot_token_.empty()) return false;

    http::HttpClient client;
    auto url = bot_api_url(api_base_, bot_token_, "getMe");
    auto resp = client.get(url);
    return resp.ok();
}

// ── Typing indicators ────────────────────────────────────────────

bool TelegramChannel::start_typing(const std::string& recipient) {
    auto [chat_id, thread_id] = parse_reply_target(recipient);
    http::HttpClient client;

    nlohmann::json body = {
        {"chat_id", chat_id},
        {"action", "typing"}
    };
    if (thread_id.has_value()) {
        body["message_thread_id"] = *thread_id;
    }

    auto url = bot_api_url(api_base_, bot_token_, "sendChatAction");
    auto resp = client.post_json(url, body);
    return resp.ok();
}

bool TelegramChannel::stop_typing(const std::string& /*recipient*/) {
    // Telegram typing indicator auto-expires; no explicit stop needed
    return true;
}

// ── Reactions ────────────────────────────────────────────────────

bool TelegramChannel::add_reaction(const std::string& channel_id,
                                     const std::string& message_id,
                                     const std::string& emoji) {
    http::HttpClient client;
    nlohmann::json body = {
        {"chat_id", channel_id},
        {"message_id", std::stoll(message_id)},
        {"reaction", nlohmann::json::array({
            {{"type", "emoji"}, {"emoji", emoji}}
        })}
    };

    auto url = bot_api_url(api_base_, bot_token_, "setMessageReaction");
    auto resp = client.post_json(url, body);
    return resp.ok();
}

bool TelegramChannel::remove_reaction(const std::string& channel_id,
                                        const std::string& message_id,
                                        const std::string& /*emoji*/) {
    http::HttpClient client;
    nlohmann::json body = {
        {"chat_id", channel_id},
        {"message_id", std::stoll(message_id)},
        {"reaction", nlohmann::json::array()}
    };

    auto url = bot_api_url(api_base_, bot_token_, "setMessageReaction");
    auto resp = client.post_json(url, body);
    return resp.ok();
}

// ── Draft editing ────────────────────────────────────────────────

bool TelegramChannel::update_draft(const std::string& channel_id,
                                     const std::string& message_id,
                                     const std::string& content) {
    http::HttpClient client;
    nlohmann::json body = {
        {"chat_id", channel_id},
        {"message_id", std::stoll(message_id)},
        {"text", content},
        {"parse_mode", "Markdown"}
    };

    auto url = bot_api_url(api_base_, bot_token_, "editMessageText");
    auto resp = client.post_json(url, body);
    return resp.ok();
}

// ── Attachments ──────────────────────────────────────────────────

bool TelegramChannel::send_attachment(const std::string& chat_id,
                                        const std::optional<std::string>& thread_id,
                                        const TelegramAttachment& attachment) {
    http::HttpClient client;

    // Determine the API method based on attachment kind
    std::string method;
    std::string field_name;
    switch (attachment.kind) {
        case TelegramAttachmentKind::Image:
            method = "sendPhoto";
            field_name = "photo";
            break;
        case TelegramAttachmentKind::Document:
            method = "sendDocument";
            field_name = "document";
            break;
        case TelegramAttachmentKind::Video:
            method = "sendVideo";
            field_name = "video";
            break;
        case TelegramAttachmentKind::Audio:
            method = "sendAudio";
            field_name = "audio";
            break;
        case TelegramAttachmentKind::Voice:
            method = "sendVoice";
            field_name = "voice";
            break;
    }

    // For URL-based attachments, we can pass the URL directly
    nlohmann::json body = {
        {"chat_id", chat_id},
        {field_name, attachment.target}
    };
    if (thread_id.has_value()) {
        body["message_thread_id"] = *thread_id;
    }

    auto url = bot_api_url(api_base_, bot_token_, method);
    auto resp = client.post_json(url, body);
    return resp.ok();
}

} // namespace channels
} // namespace zeroclaw


