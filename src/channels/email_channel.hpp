#pragma once

/// Email channel ?IMAP IDLE for incoming, SMTP for outgoing.

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include "traits.hpp"

namespace zeroclaw {
namespace channels {

/// Email channel configuration
struct EmailConfig {
    std::string imap_host;
    uint16_t imap_port = 993;
    std::string smtp_host;
    uint16_t smtp_port = 587;
    std::string username;
    std::string password;
    std::string imap_folder = "INBOX";
    uint64_t idle_timeout = 300;
    std::vector<std::string> allowed_senders;
    bool use_tls = true;
};

/// Email channel ?IMAP IDLE for instant push, SMTP for outbound
class EmailChannel : public Channel {
public:
    explicit EmailChannel(const EmailConfig& config);

    std::string name() const override { return "email"; }
    bool send(const SendMessage& message) override;
    bool listen(std::function<void(const ChannelMessage&)> callback) override;
    bool health_check() const override;

private:
    bool is_sender_allowed(const std::string& email) const;
    static std::string strip_html(const std::string& html);

    EmailConfig config_;
};

} // namespace channels
} // namespace zeroclaw

