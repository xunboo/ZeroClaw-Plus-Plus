#pragma once

/// Remaining channel stubs ?Mattermost, Lark, QQ, DingTalk, iMessage,
/// Linq, ClawdTalk, NextcloudTalk, WhatsApp Web.
/// Each follows the same Channel interface pattern.

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include "traits.hpp"

namespace zeroclaw {
namespace channels {

// ── Mattermost ───────────────────────────────────────────────────

/// Mattermost channel ?uses WebSocket + REST API
class MattermostChannel : public Channel {
public:
    MattermostChannel(const std::string& url, const std::string& token,
                      const std::string& team_id, const std::string& channel_id)
        : url_(url), token_(token), team_id_(team_id), channel_id_(channel_id) {}

    std::string name() const override { return "mattermost"; }
    bool send(const SendMessage& message) override { (void)message; return true; }
    bool listen(std::function<void(const ChannelMessage&)> callback) override { (void)callback; }
    bool health_check() const override { return !url_.empty(); }

private:
    std::string url_, token_, team_id_, channel_id_;
};

// ── Lark (Feishu) ────────────────────────────────────────────────

/// Lark (Feishu) channel ?uses bot webhook/event API
class LarkChannel : public Channel {
public:
    LarkChannel(const std::string& app_id, const std::string& app_secret)
        : app_id_(app_id), app_secret_(app_secret) {}

    std::string name() const override { return "lark"; }
    bool send(const SendMessage& message) override { (void)message; return true; }
    bool listen(std::function<void(const ChannelMessage&)> callback) override { (void)callback; }
    bool health_check() const override { return !app_id_.empty(); }

private:
    std::string app_id_, app_secret_;
};

// ── QQ ───────────────────────────────────────────────────────────

/// QQ channel ?uses QQ bot API
class QQChannel : public Channel {
public:
    QQChannel(const std::string& app_id, const std::string& app_secret, const std::string& token)
        : app_id_(app_id), app_secret_(app_secret), token_(token) {}

    std::string name() const override { return "qq"; }
    bool send(const SendMessage& message) override { (void)message; return true; }
    bool listen(std::function<void(const ChannelMessage&)> callback) override { (void)callback; }
    bool health_check() const override { return !app_id_.empty(); }

private:
    std::string app_id_, app_secret_, token_;
};

// ── DingTalk ─────────────────────────────────────────────────────

/// DingTalk channel ?uses DingTalk bot/webhook API
class DingTalkChannel : public Channel {
public:
    explicit DingTalkChannel(const std::string& access_token)
        : access_token_(access_token) {}

    std::string name() const override { return "dingtalk"; }
    bool send(const SendMessage& message) override { (void)message; return true; }
    bool listen(std::function<void(const ChannelMessage&)> callback) override { (void)callback; }
    bool health_check() const override { return !access_token_.empty(); }

private:
    std::string access_token_;
};

// ── iMessage ─────────────────────────────────────────────────────

/// iMessage channel ?uses AppleScript/sqlite on macOS
class IMessageChannel : public Channel {
public:
    IMessageChannel(const std::vector<std::string>& allowed_senders)
        : allowed_senders_(allowed_senders) {}

    std::string name() const override { return "imessage"; }
    bool send(const SendMessage& message) override { (void)message; return true; }
    bool listen(std::function<void(const ChannelMessage&)> callback) override { (void)callback; }
    bool health_check() const override {
#ifdef __APPLE__
        return true;
#else
        return false;
#endif
    }

private:
    std::vector<std::string> allowed_senders_;
};

// ── Linq ─────────────────────────────────────────────────────────

/// Linq channel ?instant messaging via Linq protocol
class LinqChannel : public Channel {
public:
    LinqChannel(const std::string& server, const std::string& token)
        : server_(server), token_(token) {}

    std::string name() const override { return "linq"; }
    bool send(const SendMessage& message) override { (void)message; return true; }
    bool listen(std::function<void(const ChannelMessage&)> callback) override { (void)callback; }
    bool health_check() const override { return !server_.empty(); }

private:
    std::string server_, token_;
};

// ── ClawdTalk ────────────────────────────────────────────────────

/// ClawdTalk configuration
struct ClawdTalkConfig {
    std::string endpoint;
    std::string token;
};

/// ClawdTalk channel ?native ZeroClaw messaging protocol
class ClawdTalkChannel : public Channel {
public:
    explicit ClawdTalkChannel(const ClawdTalkConfig& config)
        : config_(config) {}

    std::string name() const override { return "clawdtalk"; }
    bool send(const SendMessage& message) override { (void)message; return true; }
    bool listen(std::function<void(const ChannelMessage&)> callback) override { (void)callback; }
    bool health_check() const override { return !config_.endpoint.empty(); }

private:
    ClawdTalkConfig config_;
};

// ── Nextcloud Talk ───────────────────────────────────────────────

/// Nextcloud Talk channel ?polls Talk API
class NextcloudTalkChannel : public Channel {
public:
    NextcloudTalkChannel(const std::string& server, const std::string& user,
                          const std::string& token, const std::string& room_token)
        : server_(server), user_(user), token_(token), room_token_(room_token) {}

    std::string name() const override { return "nextcloud_talk"; }
    bool send(const SendMessage& message) override { (void)message; return true; }
    bool listen(std::function<void(const ChannelMessage&)> callback) override { (void)callback; }
    bool health_check() const override { return !server_.empty(); }

private:
    std::string server_, user_, token_, room_token_;
};

// ── WhatsApp Web ─────────────────────────────────────────────────

/// WhatsApp Web channel ?uses native WhatsApp Web protocol
class WhatsAppWebChannel : public Channel {
public:
    WhatsAppWebChannel(const std::string& session_path,
                        const std::vector<std::string>& allowed_numbers)
        : session_path_(session_path), allowed_numbers_(allowed_numbers) {}

    std::string name() const override { return "whatsapp_web"; }
    bool send(const SendMessage& message) override { (void)message; return true; }
    bool listen(std::function<void(const ChannelMessage&)> callback) override { (void)callback; }
    bool health_check() const override { return !session_path_.empty(); }

private:
    std::string session_path_;
    std::vector<std::string> allowed_numbers_;
};

} // namespace channels
} // namespace zeroclaw

