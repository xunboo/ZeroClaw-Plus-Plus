#pragma once

/// Signal channel ?connects to signal-cli daemon via JSON-RPC + SSE API.

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include "traits.hpp"

namespace zeroclaw {
namespace channels {

/// Signal channel using signal-cli daemon's native JSON-RPC + SSE API
class SignalChannel : public Channel {
public:
    SignalChannel(const std::string& http_url,
                  const std::string& account,
                  const std::optional<std::string>& group_id,
                  const std::vector<std::string>& allowed_from,
                  bool ignore_attachments = false,
                  bool ignore_stories = true);

    std::string name() const override { return "signal"; }
    bool send(const SendMessage& message) override;
    bool listen(std::function<void(const ChannelMessage&)> callback) override;
    bool health_check() const override;
    bool start_typing(const std::string& recipient) override;
    bool stop_typing(const std::string& recipient) override;

private:
    bool is_sender_allowed(const std::string& sender) const;
    static bool is_e164(const std::string& recipient);

    std::string http_url_;
    std::string account_;
    std::optional<std::string> group_id_;
    std::vector<std::string> allowed_from_;
    bool ignore_attachments_ = false;
    bool ignore_stories_ = true;
};

} // namespace channels
} // namespace zeroclaw

