#pragma once

/// Telegram channel ?long-polls the Bot API for updates.

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <functional>
#include <sstream>
#include "traits.hpp"

namespace zeroclaw {
namespace channels {

/// Maximum message length for Telegram messages (4096 chars)
static constexpr size_t TELEGRAM_MAX_MESSAGE_LENGTH = 4096;
static constexpr size_t TELEGRAM_CONTINUATION_OVERHEAD = 30;

/// Acknowledgment reaction emojis
static const std::vector<std::string> TELEGRAM_ACK_REACTIONS = {
    "⚡️", "🦀", "🙌", "💪", "👌", "👀", "👣"
};

/// Metadata for an incoming attachment
struct IncomingAttachment {
    std::string file_name;
    std::string local_path;
    std::string mime_type;
};

/// Type of Telegram attachment for outgoing messages
enum class TelegramAttachmentKind {
    Image,
    Document,
    Video,
    Audio,
    Voice
};

/// Parsed outgoing attachment marker
struct TelegramAttachment {
    TelegramAttachmentKind kind;
    std::string target;  // path or URL
};

/// Split a message into chunks respecting Telegram's 4096 char limit
std::vector<std::string> split_message_for_telegram(const std::string& message);

/// Parse attachment markers [IMAGE:path], [DOCUMENT:path], etc.
std::pair<std::string, std::vector<TelegramAttachment>>
parse_attachment_markers(const std::string& message);

/// Telegram channel ?connects via Bot API long-polling
class TelegramChannel : public Channel {
public:
    TelegramChannel(const std::string& bot_token,
                    const std::vector<std::string>& allowed_users,
                    bool mention_only = false);

    /// Configure workspace directory for saving downloaded attachments
    TelegramChannel& with_workspace_dir(const std::string& dir);

    /// Configure streaming mode
    TelegramChannel& with_streaming(bool enabled, uint64_t draft_interval_ms = 500);

    /// Override the Telegram Bot API base URL
    TelegramChannel& with_api_base(const std::string& api_base);

    std::string name() const override { return "telegram"; }
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
    bool update_draft(const std::string& channel_id,
                      const std::string& message_id,
                      const std::string& content) override;

private:
    /// Parse reply_target into (chat_id, optional thread_id)
    std::pair<std::string, std::optional<std::string>>
    parse_reply_target(const std::string& reply_target) const;

    /// Send an attachment to a chat
    bool send_attachment(const std::string& chat_id,
                         const std::optional<std::string>& thread_id,
                         const TelegramAttachment& attachment);

    /// Check if a user is allowed
    bool is_user_allowed(const std::string& user_id) const;

    std::string bot_token_;
    std::vector<std::string> allowed_users_;
    bool mention_only_ = false;
    std::string api_base_ = "https://api.telegram.org";
    std::string workspace_dir_;
    bool streaming_ = false;
    uint64_t draft_interval_ms_ = 500;
    mutable std::mutex mutex_;
};

} // namespace channels
} // namespace zeroclaw

