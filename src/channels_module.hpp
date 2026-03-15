#pragma once

/// Channels module — aggregates all channel implementations and provides
/// the orchestration layer for multi-channel message processing.
///
/// This module manages per-sender conversation history, concurrent message
/// processing, tool call tag stripping, and channel routing.

#include "config/config.hpp"
#include "channels/traits.hpp"
#include "providers/traits.hpp"
#include "channels/cli.hpp"
#include "channels/telegram.hpp"
#include "channels/discord.hpp"
#include "channels/slack.hpp"
#include "channels/signal.hpp"
#include "channels/irc.hpp"
#include "channels/email_channel.hpp"
#include "channels/matrix.hpp"
#include "channels/whatsapp.hpp"
#include "channels/nostr.hpp"
#include "channels/other_channels.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <functional>

namespace zeroclaw {
namespace channels {

// ── Constants ────────────────────────────────────────────────────

/// Maximum history messages to keep per sender
static constexpr size_t MAX_CHANNEL_HISTORY = 50;

/// Minimum user-message length for auto-save to memory
static constexpr size_t AUTOSAVE_MIN_MESSAGE_CHARS = 20;

/// Maximum characters per injected workspace file
static constexpr size_t CHANNEL_BOOTSTRAP_MAX_CHARS = 20000;

// ── Orchestration types ──────────────────────────────────────────

/// Per-sender conversation history
using ConversationHistoryMap = std::unordered_map<std::string, std::vector<providers::ChatMessage>>;

/// Provider/model route selection
struct ChannelRouteSelection {
    std::string provider;
    std::string model;

    bool operator==(const ChannelRouteSelection& other) const {
        return provider == other.provider && model == other.model;
    }
};

/// Runtime commands from channel users
enum class ChannelRuntimeCommandType {
    ShowProviders,
    SetProvider,
    ShowModel,
    SetModel
};

struct ChannelRuntimeCommand {
    ChannelRuntimeCommandType type;
    std::string argument;
};

// ── Utility functions ────────────────────────────────────────────

/// Strip tool-call XML tags from outgoing messages
std::string strip_tool_call_tags(const std::string& message);

/// Channel-specific delivery instructions (e.g. Telegram formatting)
std::optional<std::string> channel_delivery_instructions(const std::string& channel_name);

/// Build channel system prompt with delivery instructions
std::string build_channel_system_prompt(const std::string& base_prompt,
                                          const std::string& channel_name);

/// Normalize cached channel turns (merge consecutive same-role messages)
std::vector<providers::ChatMessage>
normalize_cached_channel_turns(const std::vector<providers::ChatMessage>& turns);

/// Check if a channel supports runtime model switching
bool supports_runtime_model_switch(const std::string& channel_name);

/// Parse a runtime command from user input
std::optional<ChannelRuntimeCommand>
parse_runtime_command(const std::string& channel_name, const std::string& content);

/// Generate a conversation memory key from a channel message
std::string conversation_memory_key(const ChannelMessage& msg);

/// Generate a conversation history key from a channel message
std::string conversation_history_key(const ChannelMessage& msg);

/// Start all background supervised channels
void start_channels(const zeroclaw::config::Config& config);

} // namespace channels
} // namespace zeroclaw
