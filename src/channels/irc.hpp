#pragma once

/// IRC over TLS channel.

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include "traits.hpp"

namespace zeroclaw {
namespace channels {

/// IRC message line length limit (512 bytes per RFC 2812)
static constexpr size_t IRC_MAX_LINE_BYTES = 512;
static constexpr size_t IRC_SENDER_PREFIX_RESERVE = 64;

/// A parsed IRC message
struct IrcMessage {
    std::optional<std::string> prefix;
    std::string command;
    std::vector<std::string> params;
    std::optional<std::string> trailing;

    /// Parse a raw IRC line
    static std::optional<IrcMessage> parse(const std::string& line);
    /// Extract nickname from prefix (nick!user@host ?nick)
    std::optional<std::string> nick() const;
};

/// Configuration for constructing an IrcChannel
struct IrcChannelConfig {
    std::string server;
    uint16_t port = 6697;
    std::string nick;
    std::string user;
    std::string real_name;
    std::optional<std::string> password;  // SASL PLAIN password
    std::vector<std::string> channels;
    std::vector<std::string> allowed_nicks;
    bool verify_tls = true;
};

/// Split a message into lines safe for IRC transmission
std::vector<std::string> split_irc_message(const std::string& message, size_t max_bytes);

/// Encode SASL PLAIN credentials: base64(\0nick\0password)
std::string encode_sasl_plain(const std::string& nick, const std::string& password);

/// IRC over TLS channel
class IrcChannel : public Channel {
public:
    explicit IrcChannel(const IrcChannelConfig& cfg);

    std::string name() const override { return "irc"; }
    bool send(const SendMessage& message) override;
    bool listen(std::function<void(const ChannelMessage&)> callback) override;
    bool health_check() const override;

private:
    bool is_user_allowed(const std::string& nick) const;

    IrcChannelConfig config_;
};

} // namespace channels
} // namespace zeroclaw

