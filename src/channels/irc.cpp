#include "irc.hpp"
#include <sstream>
#include <algorithm>

namespace zeroclaw {
namespace channels {

// ── IrcMessage ───────────────────────────────────────────────────

std::optional<IrcMessage> IrcMessage::parse(const std::string& line) {
    if (line.empty()) return std::nullopt;

    std::string remaining = line;
    // Trim trailing \r\n
    while (!remaining.empty() && (remaining.back() == '\r' || remaining.back() == '\n')) {
        remaining.pop_back();
    }

    IrcMessage msg;

    // Parse prefix
    if (!remaining.empty() && remaining[0] == ':') {
        auto space = remaining.find(' ');
        if (space == std::string::npos) return std::nullopt;
        msg.prefix = remaining.substr(1, space - 1);
        remaining = remaining.substr(space + 1);
    }

    // Parse command and params
    while (!remaining.empty()) {
        if (remaining[0] == ':') {
            msg.trailing = remaining.substr(1);
            break;
        }
        auto space = remaining.find(' ');
        if (space == std::string::npos) {
            if (msg.command.empty()) msg.command = remaining;
            else msg.params.push_back(remaining);
            break;
        }
        auto token = remaining.substr(0, space);
        if (msg.command.empty()) msg.command = token;
        else msg.params.push_back(token);
        remaining = remaining.substr(space + 1);
    }

    if (msg.command.empty()) return std::nullopt;
    return msg;
}

std::optional<std::string> IrcMessage::nick() const {
    if (!prefix.has_value()) return std::nullopt;
    auto bang = prefix->find('!');
    if (bang != std::string::npos) return prefix->substr(0, bang);
    return *prefix;
}

std::vector<std::string> split_irc_message(const std::string& message, size_t max_bytes) {
    std::vector<std::string> lines;
    std::istringstream stream(message);
    std::string line;

    while (std::getline(stream, line, '\n')) {
        // Trim trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        while (line.size() > max_bytes) {
            // Find safe UTF-8 split point
            size_t split_at = max_bytes;
            while (split_at > 0 && (static_cast<unsigned char>(line[split_at]) & 0xC0) == 0x80) {
                --split_at;
            }
            if (split_at == 0) split_at = max_bytes;
            lines.push_back(line.substr(0, split_at));
            line = line.substr(split_at);
        }
        if (!line.empty()) lines.push_back(line);
    }

    return lines;
}

std::string encode_sasl_plain(const std::string& nick, const std::string& password) {
    // SASL PLAIN: base64(\0nick\0password)
    std::string plain;
    plain += '\0';
    plain += nick;
    plain += '\0';
    plain += password;

    // Simple base64 encode
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    size_t i = 0;
    while (i < plain.size()) {
        uint32_t a = static_cast<uint8_t>(plain[i++]);
        uint32_t b = (i < plain.size()) ? static_cast<uint8_t>(plain[i++]) : 0;
        uint32_t c = (i < plain.size()) ? static_cast<uint8_t>(plain[i++]) : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;
        encoded += b64[(triple >> 18) & 0x3F];
        encoded += b64[(triple >> 12) & 0x3F];
        encoded += (i > plain.size() + 1) ? '=' : b64[(triple >> 6) & 0x3F];
        encoded += (i > plain.size()) ? '=' : b64[triple & 0x3F];
    }
    return encoded;
}

// ── IrcChannel ───────────────────────────────────────────────────

IrcChannel::IrcChannel(const IrcChannelConfig& cfg)
    : config_(cfg) {}

bool IrcChannel::is_user_allowed(const std::string& nick) const {
    if (config_.allowed_nicks.empty()) return true;
    for (const auto& allowed : config_.allowed_nicks) {
        if (allowed == "*" || allowed == nick) return true;
    }
    return false;
}

bool IrcChannel::send(const SendMessage& message) {
    size_t max_bytes = IRC_MAX_LINE_BYTES - IRC_SENDER_PREFIX_RESERVE
                       - message.recipient.size() - 12;
    auto lines = split_irc_message(message.content, max_bytes);
    for (const auto& line : lines) {
        // Would send: PRIVMSG <recipient> :<line>\r\n
        (void)line;
    }
    return true;
}

bool IrcChannel::listen(std::function<void(const ChannelMessage&)> /*callback*/) {
    // Would: TLS connect, NICK/USER/SASL, JOIN channels, parse PRIVMSG events

    return true;
}

bool IrcChannel::health_check() const {
    return !config_.server.empty() && !config_.nick.empty();
}

} // namespace channels
} // namespace zeroclaw


