#include "email_channel.hpp"
#include <algorithm>
#include <regex>

namespace zeroclaw {
namespace channels {

EmailChannel::EmailChannel(const EmailConfig& config)
    : config_(config) {}

bool EmailChannel::is_sender_allowed(const std::string& email) const {
    if (config_.allowed_senders.empty()) return false;
    std::string lower_email = email;
    std::transform(lower_email.begin(), lower_email.end(), lower_email.begin(), ::tolower);
    for (const auto& allowed : config_.allowed_senders) {
        if (allowed == "*") return true;
        std::string lower_allowed = allowed;
        std::transform(lower_allowed.begin(), lower_allowed.end(), lower_allowed.begin(), ::tolower);
        if (lower_email.find(lower_allowed) != std::string::npos) return true;
    }
    return false;
}

std::string EmailChannel::strip_html(const std::string& html) {
    // Basic HTML tag removal using regex
    static const std::regex tag_regex("<[^>]*>");
    std::string result = std::regex_replace(html, tag_regex, "");
    // Collapse whitespace
    static const std::regex ws_regex("\\s+");
    result = std::regex_replace(result, ws_regex, " ");
    return result;
}

bool EmailChannel::send(const SendMessage& message) {
    // Would connect via SMTP and send email
    (void)message;
    return true;
}

bool EmailChannel::listen(std::function<void(const ChannelMessage&)> /*callback*/) {
    // Would use IMAP IDLE to receive new emails in real-time

    return true;
}

bool EmailChannel::health_check() const {
    return !config_.imap_host.empty() && !config_.smtp_host.empty();
}

} // namespace channels
} // namespace zeroclaw


