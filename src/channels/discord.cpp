#include "discord.hpp"
#include "../http/http_client.hpp"
#include <algorithm>

namespace zeroclaw {
namespace channels {

static const std::string DISCORD_API_BASE = "https://discord.com/api/v10";

std::vector<std::string> split_message_for_discord(const std::string& message) {
    std::vector<std::string> chunks;
    if (message.size() <= DISCORD_MAX_MESSAGE_LENGTH) {
        chunks.push_back(message);
        return chunks;
    }

    size_t pos = 0;
    while (pos < message.size()) {
        size_t remaining = message.size() - pos;
        if (remaining <= DISCORD_MAX_MESSAGE_LENGTH) {
            chunks.push_back(message.substr(pos));
            break;
        }

        size_t split_at = pos + DISCORD_MAX_MESSAGE_LENGTH;
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

std::string encode_emoji_for_discord(const std::string& emoji) {
    if (emoji.find(':') != std::string::npos) return emoji;

    std::string encoded;
    for (unsigned char c : emoji) {
        char buf[4];
        std::snprintf(buf, sizeof(buf), "%%%02X", c);
        encoded += buf;
    }
    return encoded;
}

std::optional<std::string> normalize_incoming_content(
    const std::string& content, bool mention_only, const std::string& bot_user_id) {
    if (bot_user_id.empty()) return content;

    std::string tag1 = "<@" + bot_user_id + ">";
    std::string tag2 = "<@!" + bot_user_id + ">";

    bool has_mention = content.find(tag1) != std::string::npos ||
                       content.find(tag2) != std::string::npos;

    if (mention_only && !has_mention) return std::nullopt;

    std::string result = content;
    size_t p;
    while ((p = result.find(tag1)) != std::string::npos) {
        result.erase(p, tag1.size());
    }
    while ((p = result.find(tag2)) != std::string::npos) {
        result.erase(p, tag2.size());
    }

    size_t s = result.find_first_not_of(" \t\n\r");
    size_t e = result.find_last_not_of(" \t\n\r");
    if (s == std::string::npos) return "";
    return result.substr(s, e - s + 1);
}

// ── DiscordChannel ───────────────────────────────────────────────

DiscordChannel::DiscordChannel(const std::string& bot_token,
                                const std::optional<std::string>& guild_id,
                                const std::vector<std::string>& allowed_users,
                                bool listen_to_bots,
                                bool mention_only)
    : bot_token_(bot_token), guild_id_(guild_id), allowed_users_(allowed_users),
      listen_to_bots_(listen_to_bots), mention_only_(mention_only) {
    auto id = bot_user_id_from_token(bot_token);
    if (id.has_value()) bot_user_id_ = *id;
}

bool DiscordChannel::is_user_allowed(const std::string& user_id) const {
    if (allowed_users_.empty()) return false;
    for (const auto& allowed : allowed_users_) {
        if (allowed == "*" || allowed == user_id) return true;
    }
    return false;
}

std::optional<std::string> DiscordChannel::bot_user_id_from_token(const std::string& token) {
    auto dot = token.find('.');
    if (dot == std::string::npos) return std::nullopt;
    // Base64 decode the first segment to get the bot user ID
    // Simplified ?full implementation would do proper base64 decoding
    return std::nullopt;
}

bool DiscordChannel::send(const SendMessage& message) {
    http::HttpClient client;
    client.with_bearer_token(bot_token_);
    client.with_header("Content-Type", "application/json");

    auto chunks = split_message_for_discord(message.content);
    for (const auto& chunk : chunks) {
        nlohmann::json body = {{"content", chunk}};
        std::string url = DISCORD_API_BASE + "/channels/" +
                           message.recipient + "/messages";
        auto resp = client.post_json(url, body);
        if (!resp.ok()) return false;
    }
    return true;
}

bool DiscordChannel::listen(std::function<void(const ChannelMessage&)> /*callback*/) {
    // Discord requires WebSocket Gateway connection for real-time events.
    // REST-only polling is not supported by Discord API.
    // A full implementation would:
    //   1. GET /gateway/bot to get WebSocket URL
    //   2. Connect via WebSocket, send IDENTIFY payload
    //   3. Handle HEARTBEAT and MESSAGE_CREATE events
    // This is left as a WebSocket integration point.

    return true;
}

bool DiscordChannel::health_check() const {
    if (bot_token_.empty()) return false;

    http::HttpClient client;
    client.with_bearer_token(bot_token_);

    std::string url = DISCORD_API_BASE + "/users/@me";
    auto resp = client.get(url);
    return resp.ok();
}

bool DiscordChannel::start_typing(const std::string& recipient) {
    http::HttpClient client;
    client.with_bearer_token(bot_token_);

    std::string url = DISCORD_API_BASE + "/channels/" + recipient + "/typing";
    auto resp = client.post(url, "", "application/json");
    return resp.ok();
}

bool DiscordChannel::stop_typing(const std::string& /*recipient*/) {
    // Discord typing indicator auto-expires after ~10s
    return true;
}

bool DiscordChannel::add_reaction(const std::string& channel_id,
                                    const std::string& message_id,
                                    const std::string& emoji) {
    http::HttpClient client;
    client.with_bearer_token(bot_token_);

    std::string encoded = encode_emoji_for_discord(emoji);
    std::string url = DISCORD_API_BASE + "/channels/" + channel_id +
                       "/messages/" + message_id +
                       "/reactions/" + encoded + "/@me";
    auto resp = client.put(url, "", "application/json");
    return resp.ok();
}

bool DiscordChannel::remove_reaction(const std::string& channel_id,
                                       const std::string& message_id,
                                       const std::string& emoji) {
    http::HttpClient client;
    client.with_bearer_token(bot_token_);

    std::string encoded = encode_emoji_for_discord(emoji);
    std::string url = DISCORD_API_BASE + "/channels/" + channel_id +
                       "/messages/" + message_id +
                       "/reactions/" + encoded + "/@me";
    auto resp = client.delete_(url);
    return resp.ok();
}

} // namespace channels
} // namespace zeroclaw


