#include "signal.hpp"

namespace zeroclaw {
namespace channels {

SignalChannel::SignalChannel(const std::string& http_url,
                              const std::string& account,
                              const std::optional<std::string>& group_id,
                              const std::vector<std::string>& allowed_from,
                              bool ignore_attachments,
                              bool ignore_stories)
    : http_url_(http_url), account_(account), group_id_(group_id),
      allowed_from_(allowed_from), ignore_attachments_(ignore_attachments),
      ignore_stories_(ignore_stories) {}

bool SignalChannel::is_sender_allowed(const std::string& sender) const {
    if (allowed_from_.empty()) return false;
    for (const auto& allowed : allowed_from_) {
        if (allowed == "*" || allowed == sender) return true;
    }
    return false;
}

bool SignalChannel::is_e164(const std::string& recipient) {
    return !recipient.empty() && recipient[0] == '+' && recipient.size() >= 8;
}

bool SignalChannel::send(const SendMessage& message) {
    // Would send via JSON-RPC to signal-cli daemon
    (void)message;
    return true;
}

bool SignalChannel::listen(std::function<void(const ChannelMessage&)> /*callback*/) {
    // Would connect to SSE endpoint /api/v1/events and parse envelopes

    return true;
}

bool SignalChannel::health_check() const {
    return !http_url_.empty() && !account_.empty();
}

bool SignalChannel::start_typing(const std::string& /*recipient*/) {
    // Would call sendTypingIndicator via JSON-RPC
    return true;
}

bool SignalChannel::stop_typing(const std::string& /*recipient*/) {
    return true;
}

} // namespace channels
} // namespace zeroclaw


