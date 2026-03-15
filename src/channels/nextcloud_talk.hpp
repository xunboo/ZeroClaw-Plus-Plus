#pragma once

/// Nextcloud Talk channel — webhook mode.
///
/// Incoming messages are received by the gateway endpoint /nextcloud-talk.
/// Outbound replies are sent through Nextcloud Talk OCS API.

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include "traits.hpp"
#include "../http/http_client.hpp"
#include <nlohmann/json.hpp>

namespace zeroclaw {
namespace channels {

/// Nextcloud Talk channel using webhook-based ingestion and OCS API for sending.
class NextcloudTalkChannel : public Channel {
public:
    NextcloudTalkChannel(const std::string& base_url,
                         const std::string& app_token,
                         const std::vector<std::string>& allowed_users);

    std::string name() const override { return "nextcloud_talk"; }
    bool send(const SendMessage& message) override;
    bool listen(std::function<void(const ChannelMessage&)> callback) override;
    bool health_check() const override;
    bool start_typing(const std::string& recipient) override { (void)recipient; return true; }
    bool stop_typing(const std::string& recipient) override { (void)recipient; return true; }

    /// Parse a Nextcloud Talk webhook payload into channel messages.
    ///
    /// Accepts event types: "message" (case-insensitive) or "Create" (case-insensitive).
    /// Skips bot-originated messages, non-comment message types, and system messages.
    std::vector<ChannelMessage> parse_webhook_payload(const nlohmann::json& payload) const;

    /// Verify HMAC-SHA256 signature from Nextcloud Talk webhook request.
    bool verify_signature(const std::string& body, const std::string& signature) const;

private:
    bool is_user_allowed(const std::string& actor_id) const;

    static uint64_t parse_timestamp_secs(const nlohmann::json& value);
    static std::optional<std::string> value_to_string(const nlohmann::json& value);

    std::string base_url_;
    std::string app_token_;
    std::vector<std::string> allowed_users_;
};

} // namespace channels
} // namespace zeroclaw
