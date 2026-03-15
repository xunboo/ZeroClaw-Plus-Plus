#pragma once

/// Slack channel — polls conversations.history via Web API.
/// Supports mention-only filtering, multi-channel scope, and group reply policy.

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <unordered_map>
#include "traits.hpp"
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace channels {

/// Slack channel — polls conversations.history via Web API
class SlackChannel : public Channel {
public:
    SlackChannel(const std::string& bot_token,
                 const std::optional<std::string>& channel_id,
                 const std::vector<std::string>& allowed_users,
                 // Matching Rust: additional fields
                 bool mention_only = false,
                 const std::vector<std::string>& group_reply_allowed_sender_ids = {},
                 const std::vector<std::string>& channel_ids = {});

    std::string name() const override { return "slack"; }
    bool send(const SendMessage& message) override;
    bool listen(std::function<void(const ChannelMessage&)> callback) override;
    bool health_check() const override;

private:
    bool is_user_allowed(const std::string& user_id) const;
    std::optional<std::string> get_bot_user_id() const;
    std::vector<std::string> list_accessible_channels() const;

    /// Matching Rust: is_group_channel_id() — Slack group/public channels start with C or G
    bool is_group_channel_id(const std::string& channel_id) const;

    /// Matching Rust: contains_bot_mention() — checks if text contains <@BOT_USER_ID>
    bool contains_bot_mention(const std::string& text, const std::string& bot_user_id) const;

    /// Matching Rust: strip_bot_mentions() — removes all <@BOT_USER_ID> mentions from text
    std::string strip_bot_mentions(const std::string& text, const std::string& bot_user_id) const;

    /// Matching Rust: inbound_thread_ts() — returns thread_ts if msg is in a thread
    std::optional<std::string> inbound_thread_ts(const nlohmann::json& msg) const;

    /// Matching Rust: scoped_channel_ids() — deduped union of channel_id_ and channel_ids_
    std::vector<std::string> scoped_channel_ids() const;

    std::string bot_token_;
    std::optional<std::string> channel_id_;
    std::vector<std::string> allowed_users_;

    // Fields added to match Rust implementation
    bool mention_only_;
    std::vector<std::string> group_reply_allowed_sender_ids_;
    std::vector<std::string> channel_ids_;
};

} // namespace channels
} // namespace zeroclaw
