#pragma once

/// Discord channel ?connects via Gateway WebSocket for real-time messages.

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <functional>
#include "traits.hpp"

namespace zeroclaw {
namespace channels {

/// Discord's maximum message length for regular messages
static constexpr size_t DISCORD_MAX_MESSAGE_LENGTH = 2000;

/// Acknowledgment reaction emojis
static const std::vector<std::string> DISCORD_ACK_REACTIONS = {
    "⚡️", "🦀", "🙌", "💪", "👌", "👀", "👣"
};

/// Split a message into chunks respecting Discord's 2000-character limit
std::vector<std::string> split_message_for_discord(const std::string& message);

/// URL-encode a Unicode emoji for use in Discord reaction API paths
std::string encode_emoji_for_discord(const std::string& emoji);

/// Normalize incoming content: strip bot mentions, check mention_only mode
std::optional<std::string> normalize_incoming_content(
    const std::string& content, bool mention_only, const std::string& bot_user_id);

/// Discord channel ?connects via Gateway WebSocket
class DiscordChannel : public Channel {
public:
    DiscordChannel(const std::string& bot_token,
                   const std::optional<std::string>& guild_id,
                   const std::vector<std::string>& allowed_users,
                   bool listen_to_bots = false,
                   bool mention_only = false);

    std::string name() const override { return "discord"; }
    bool send(const SendMessage& message) override;
    bool listen(std::function<void(const ChannelMessage&)> callback) override;
    bool health_check() const override;
    bool start_typing(const std::string& recipient) override;
    bool stop_typing(const std::string& recipient) override;
    bool add_reaction(const std::string& channel_id,
                      const std::string& message_id,
                      const std::string& emoji) override;
    bool remove_reaction(const std::string& channel_id,
                          const std::string& message_id,
                          const std::string& emoji) override;

private:
    /// Check if a Discord user ID is in the allowlist
    bool is_user_allowed(const std::string& user_id) const;

    /// Extract bot user ID from token (base64 decode first segment)
    static std::optional<std::string> bot_user_id_from_token(const std::string& token);

    std::string bot_token_;
    std::optional<std::string> guild_id_;
    std::vector<std::string> allowed_users_;
    bool listen_to_bots_ = false;
    bool mention_only_ = false;
    mutable std::string bot_user_id_;
    mutable std::mutex mutex_;
};

} // namespace channels
} // namespace zeroclaw

