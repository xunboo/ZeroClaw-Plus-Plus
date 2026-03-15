#pragma once

/// Channel traits ?core types for messaging platform interactions.
///
/// This module defines the unified interface for communicating through
/// various messaging platforms (CLI, Discord, Telegram, Slack, etc.).

#include <string>
#include <optional>
#include <cstdint>
#include <functional>
#include <memory>

namespace zeroclaw {
namespace channels {

/// A message received from or sent to a channel
struct ChannelMessage {
    std::string id;
    std::string sender;
    std::string reply_target;
    std::string content;
    std::string channel;
    uint64_t timestamp = 0;
    /// Platform thread identifier (e.g. Slack `ts`, Discord thread ID).
    /// When set, replies should be posted as threaded responses.
    std::optional<std::string> thread_ts;
};

/// Message to send through a channel
struct SendMessage {
    std::string content;
    std::string recipient;
    std::optional<std::string> subject;
    /// Platform thread identifier for threaded replies (e.g. Slack `thread_ts`).
    std::optional<std::string> thread_ts;

    /// Create a new message with content and recipient
    SendMessage(const std::string& content, const std::string& recipient)
        : content(content), recipient(recipient) {}

    /// Create a new message with content, recipient, and subject
    SendMessage(const std::string& content, const std::string& recipient,
                const std::string& subject)
        : content(content), recipient(recipient), subject(subject) {}

    /// Set the thread identifier for threaded replies.
    SendMessage& in_thread(const std::optional<std::string>& ts) {
        thread_ts = ts;
        return *this;
    }
};

/// Core channel trait ?implement for any messaging platform
class Channel {
public:
    virtual ~Channel() = default;

    /// Human-readable channel name
    virtual std::string name() const = 0;

    /// Send a message through this channel
    virtual bool send(const SendMessage& message) = 0;

    /// Start listening for incoming messages (long-running).
    /// Calls the callback for each received message.
    /// Returns false on failure.
    virtual bool listen(std::function<void(const ChannelMessage&)> callback) = 0;

    /// Check if channel is healthy
    virtual bool health_check() const { return true; }

    /// Signal that the bot is processing a response (e.g. "typing" indicator).
    virtual bool start_typing(const std::string& /*recipient*/) { return true; }

    /// Stop any active typing indicator.
    virtual bool stop_typing(const std::string& /*recipient*/) { return true; }

    /// Whether this channel supports progressive message updates via draft edits.
    virtual bool supports_draft_updates() const { return false; }

    /// Send an initial draft message. Returns a platform-specific message ID for later edits.
    virtual std::optional<std::string> send_draft(const SendMessage& /*message*/) {
        return std::nullopt;
    }

    /// Update a previously sent draft message with new accumulated content.
    virtual bool update_draft(const std::string& /*recipient*/,
                               const std::string& /*message_id*/,
                               const std::string& /*text*/) {
        return true;
    }

    /// Finalize a draft with the complete response (e.g. apply Markdown formatting).
    virtual bool finalize_draft(const std::string& /*recipient*/,
                                 const std::string& /*message_id*/,
                                 const std::string& /*text*/) {
        return true;
    }

    /// Cancel and remove a previously sent draft message if the channel supports it.
    virtual bool cancel_draft(const std::string& /*recipient*/,
                               const std::string& /*message_id*/) {
        return true;
    }

    /// Add a reaction (emoji) to a message.
    virtual bool add_reaction(const std::string& /*channel_id*/,
                               const std::string& /*message_id*/,
                               const std::string& /*emoji*/) {
        return true;
    }

    /// Remove a reaction (emoji) from a message previously added by this bot.
    virtual bool remove_reaction(const std::string& /*channel_id*/,
                                  const std::string& /*message_id*/,
                                  const std::string& /*emoji*/) {
        return true;
    }
};

} // namespace channels
} // namespace zeroclaw

