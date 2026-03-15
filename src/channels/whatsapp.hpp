#pragma once

/// WhatsApp channel ?uses WhatsApp Business Cloud API (webhook-based).

#include <string>
#include <vector>
#include <functional>
#include "traits.hpp"
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace channels {

/// WhatsApp Business Cloud API channel
class WhatsAppChannel : public Channel {
public:
    WhatsAppChannel(const std::string& access_token,
                    const std::string& endpoint_id,
                    const std::string& verify_token,
                    const std::vector<std::string>& allowed_numbers);

    std::string name() const override { return "whatsapp"; }
    bool send(const SendMessage& message) override;
    bool listen(std::function<void(const ChannelMessage&)> callback) override;
    bool health_check() const override;

    /// Get the verify token for webhook verification
    const std::string& verify_token() const { return verify_token_; }

    /// Parse an incoming webhook payload from Meta and extract messages
    std::vector<ChannelMessage> parse_webhook_payload(const nlohmann::json& payload) const;

private:
    bool is_number_allowed(const std::string& phone) const;

    std::string access_token_;
    std::string endpoint_id_;
    std::string verify_token_;
    std::vector<std::string> allowed_numbers_;
};

} // namespace channels
} // namespace zeroclaw

