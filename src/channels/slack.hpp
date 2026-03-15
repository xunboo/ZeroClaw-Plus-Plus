#pragma once

/// Slack channel ?polls conversations.history via Web API.

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include "traits.hpp"

namespace zeroclaw {
namespace channels {

/// Slack channel ?polls conversations.history via Web API
class SlackChannel : public Channel {
public:
    SlackChannel(const std::string& bot_token,
                 const std::optional<std::string>& channel_id,
                 const std::vector<std::string>& allowed_users);

    std::string name() const override { return "slack"; }
    bool send(const SendMessage& message) override;
    bool listen(std::function<void(const ChannelMessage&)> callback) override;
    bool health_check() const override;

private:
    bool is_user_allowed(const std::string& user_id) const;
    std::optional<std::string> get_bot_user_id() const;
    std::vector<std::string> list_accessible_channels() const;

    std::string bot_token_;
    std::optional<std::string> channel_id_;
    std::vector<std::string> allowed_users_;
};

} // namespace channels
} // namespace zeroclaw

