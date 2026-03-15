#pragma once

/// Matrix channel ?Matrix Client-Server API with sync and encrypted room support.

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <deque>
#include <set>
#include "traits.hpp"

namespace zeroclaw {
namespace channels {

/// Matrix channel for the Matrix Client-Server API
class MatrixChannel : public Channel {
public:
    MatrixChannel(const std::string& homeserver,
                  const std::string& access_token,
                  const std::string& room_id,
                  const std::vector<std::string>& allowed_users);

    /// Configure E2EE session hints
    MatrixChannel& with_session_hint(const std::optional<std::string>& owner,
                                      const std::optional<std::string>& device_id);

    std::string name() const override { return "matrix"; }
    bool send(const SendMessage& message) override;
    bool listen(std::function<void(const ChannelMessage&)> callback) override;
    bool health_check() const override;

private:
    bool is_user_allowed(const std::string& sender) const;
    static bool is_sender_allowed(const std::vector<std::string>& allowed, const std::string& sender);
    static std::string encode_path_segment(const std::string& value);
    std::string auth_header_value() const;

    std::string homeserver_;
    std::string access_token_;
    std::string room_id_;
    std::vector<std::string> allowed_users_;
    std::optional<std::string> owner_hint_;
    std::optional<std::string> device_id_hint_;
    std::deque<std::string> recent_event_ids_;
    std::set<std::string> recent_event_lookup_;
};

} // namespace channels
} // namespace zeroclaw

