#include "matrix.hpp"
#include <sstream>

namespace zeroclaw {
namespace channels {

MatrixChannel::MatrixChannel(const std::string& homeserver,
                              const std::string& access_token,
                              const std::string& room_id,
                              const std::vector<std::string>& allowed_users)
    : homeserver_(homeserver), access_token_(access_token),
      room_id_(room_id), allowed_users_(allowed_users) {}

MatrixChannel& MatrixChannel::with_session_hint(
    const std::optional<std::string>& owner,
    const std::optional<std::string>& device_id) {
    owner_hint_ = owner;
    device_id_hint_ = device_id;
    return *this;
}

bool MatrixChannel::is_user_allowed(const std::string& sender) const {
    return is_sender_allowed(allowed_users_, sender);
}

bool MatrixChannel::is_sender_allowed(const std::vector<std::string>& allowed,
                                        const std::string& sender) {
    if (allowed.empty()) return false;
    for (const auto& a : allowed) {
        if (a == "*" || a == sender) return true;
    }
    return false;
}

std::string MatrixChannel::encode_path_segment(const std::string& value) {
    std::ostringstream oss;
    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            oss << static_cast<char>(c);
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            oss << buf;
        }
    }
    return oss.str();
}

std::string MatrixChannel::auth_header_value() const {
    return "Bearer " + access_token_;
}

bool MatrixChannel::send(const SendMessage& message) {
    // Would PUT to /_matrix/client/r0/rooms/{roomId}/send/m.room.message/{txnId}
    (void)message;
    return true;
}

bool MatrixChannel::listen(std::function<void(const ChannelMessage&)> /*callback*/) {
    // Would use /_matrix/client/r0/sync with long-polling

    return true;
}

bool MatrixChannel::health_check() const {
    return !homeserver_.empty() && !access_token_.empty();
}

} // namespace channels
} // namespace zeroclaw


