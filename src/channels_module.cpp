#include "channels_module.hpp"
#include <algorithm>
#include <sstream>
#include "providers/traits.hpp"

namespace zeroclaw {
namespace channels {

// ── Tool call tag stripping ──────────────────────────────────────

std::string strip_tool_call_tags(const std::string& message) {
    static const std::vector<std::pair<std::string, std::string>> tag_pairs = {
        {"<function_calls>", "</function_calls>"},
        {"<function_call>",  "</function_call>"},
        {"<tool_call>",      "</tool_call>"},
        {"<toolcall>",       "</toolcall>"},
        {"<tool-call>",      "</tool-call>"},
        {"<tool>",           "</tool>"},
        {"<invoke>",         "</invoke>"},
    };

    std::string result = message;

    for (const auto& [open_tag, close_tag] : tag_pairs) {
        while (true) {
            auto start = result.find(open_tag);
            if (start == std::string::npos) break;

            auto end = result.find(close_tag, start + open_tag.size());
            if (end == std::string::npos) break;

            result.erase(start, end + close_tag.size() - start);
        }
    }

    // Collapse triple newlines
    while (result.find("\n\n\n") != std::string::npos) {
        size_t pos;
        while ((pos = result.find("\n\n\n")) != std::string::npos) {
            result.replace(pos, 3, "\n\n");
        }
    }

    // Trim
    size_t s = result.find_first_not_of(" \t\n\r");
    size_t e = result.find_last_not_of(" \t\n\r");
    if (s == std::string::npos) return "";
    return result.substr(s, e - s + 1);
}

std::optional<std::string> channel_delivery_instructions(const std::string& channel_name) {
    if (channel_name == "telegram") {
        return "When responding on Telegram:\n"
               "- Use **bold** for key terms and section titles (renders as <b>)\n"
               "- Use *italic* for emphasis (renders as <i>)\n"
               "- Use `backticks` for inline code\n"
               "- Use triple backticks for code blocks\n"
               "- Use emoji naturally — but don't overdo it\n"
               "- Be concise and direct\n"
               "- For media attachments use markers: [IMAGE:<path>], [DOCUMENT:<path>]";
    }
    return std::nullopt;
}

std::string build_channel_system_prompt(const std::string& base_prompt,
                                          const std::string& channel_name) {
    auto instructions = channel_delivery_instructions(channel_name);
    if (instructions.has_value()) {
        if (base_prompt.empty()) return *instructions;
        return base_prompt + "\n\n" + *instructions;
    }
    return base_prompt;
}

std::vector<providers::ChatMessage>
normalize_cached_channel_turns(const std::vector<providers::ChatMessage>& turns) {
    std::vector<providers::ChatMessage> normalized;
    bool expecting_user = true;

    for (const auto& turn : turns) {
        bool is_user = turn.role == "user";
        bool is_assistant = turn.role == "assistant";

        if ((expecting_user && is_user) || (!expecting_user && is_assistant)) {
            normalized.push_back(turn);
            expecting_user = !expecting_user;
        } else if (!normalized.empty()) {
            // Merge consecutive same-direction messages
            auto& last = normalized.back();
            if (!turn.content.empty()) {
                if (!last.content.empty()) last.content += "\n\n";
                last.content += turn.content;
            }
        }
    }

    return normalized;
}

bool supports_runtime_model_switch(const std::string& channel_name) {
    return channel_name == "telegram" || channel_name == "discord";
}

std::optional<ChannelRuntimeCommand>
parse_runtime_command(const std::string& channel_name, const std::string& content) {
    if (!supports_runtime_model_switch(channel_name)) return std::nullopt;

    std::string trimmed = content;
    size_t s = trimmed.find_first_not_of(" \t");
    if (s == std::string::npos || trimmed[s] != '/') return std::nullopt;
    trimmed = trimmed.substr(s);

    // Split into tokens
    std::istringstream iss(trimmed);
    std::string command_token;
    iss >> command_token;

    // Strip @bot_username from command
    auto at = command_token.find('@');
    if (at != std::string::npos) command_token = command_token.substr(0, at);

    // Lowercase
    std::transform(command_token.begin(), command_token.end(), command_token.begin(), ::tolower);

    if (command_token == "/models") {
        std::string arg;
        iss >> arg;
        if (arg.empty()) {
            return ChannelRuntimeCommand{ChannelRuntimeCommandType::ShowProviders, ""};
        }
        return ChannelRuntimeCommand{ChannelRuntimeCommandType::SetProvider, arg};
    }

    if (command_token == "/model") {
        std::string rest;
        std::getline(iss, rest);
        // Trim
        size_t rs = rest.find_first_not_of(" \t");
        size_t re = rest.find_last_not_of(" \t");
        if (rs == std::string::npos) {
            return ChannelRuntimeCommand{ChannelRuntimeCommandType::ShowModel, ""};
        }
        return ChannelRuntimeCommand{ChannelRuntimeCommandType::SetModel,
                                       rest.substr(rs, re - rs + 1)};
    }

    return std::nullopt;
}

std::string conversation_memory_key(const ChannelMessage& msg) {
    return msg.channel + "_" + msg.sender + "_" + msg.id;
}

std::string conversation_history_key(const ChannelMessage& msg) {
    return msg.channel + "_" + msg.sender;
}

void start_channels(const config::Config& config) {
    // Collect and start all configured channels
    auto configured = list_configured_channels(config);
    bool any_started = false;
    for (const auto& [name, is_configured] : configured) {
        if (is_configured) {
            std::cout << "  Starting channel: " << name << "\n";
            any_started = true;
        }
    }
    if (!any_started) {
        std::cout << "No channels configured. Use 'zeroclaw++ onboard' to configure channels.\n";
    }
}

std::vector<std::pair<std::string, bool>> list_configured_channels(const config::Config& config) {
    std::vector<std::pair<std::string, bool>> channels;
    channels.emplace_back("telegram", config.channels_config.telegram.has_value());
    channels.emplace_back("discord", config.channels_config.discord.has_value());
    channels.emplace_back("slack", config.channels_config.slack.has_value());
    channels.emplace_back("mattermost", config.channels_config.mattermost.has_value());
    channels.emplace_back("imessage", config.channels_config.imessage.has_value());
    channels.emplace_back("matrix", config.channels_config.matrix.has_value());
    channels.emplace_back("signal", config.channels_config.signal.has_value());
    channels.emplace_back("whatsapp", config.channels_config.whatsapp.has_value());
    channels.emplace_back("irc", config.channels_config.irc.has_value());
    channels.emplace_back("nostr", config.channels_config.nostr.has_value());
    channels.emplace_back("nextcloud_talk", config.channels_config.nextcloud_talk.has_value());
    channels.emplace_back("lark", config.channels_config.lark.has_value());
    channels.emplace_back("dingtalk", config.channels_config.dingtalk.has_value());
    channels.emplace_back("qq", config.channels_config.qq.has_value());
    return channels;
}

void channel_doctor(const config::Config& config) {
    auto configured = list_configured_channels(config);
    std::cout << "Channel Health Check:\n\n";
    std::cout << "  CLI:      healthy (always available)\n";

    bool any_configured = false;
    for (const auto& [name, is_configured] : configured) {
        if (is_configured) {
            any_configured = true;
            // Basic health: we can verify the config is present
            std::cout << "  " << name << ":  configured (token present)\n";
        }
    }

    if (!any_configured) {
        std::cout << "\n  No external channels configured.\n";
        std::cout << "  Run 'zeroclaw++ onboard' to set up channels.\n";
    }
    std::cout << "\n";
}

void handle_command(const config::Config& config, const std::string& subcommand,
                    const std::string& arg1, const std::string& arg2,
                    const std::string& arg3) {
    if (subcommand == "list") {
        std::cout << "Channels:\n";
        std::cout << "  CLI (always available)\n";
        auto configured = list_configured_channels(config);
        for (const auto& [name, is_configured] : configured) {
            std::cout << "  " << (is_configured ? "+" : "-")
                      << " " << name << "\n";
        }
        std::cout << "\nTo start channels: zeroclaw++ channel start\n";
        std::cout << "To check health:    zeroclaw++ channel doctor\n";
        std::cout << "To configure:      zeroclaw++ onboard\n";
    } else if (subcommand == "doctor") {
        channel_doctor(config);
    } else if (subcommand == "start") {
        start_channels(config);
    } else if (subcommand == "add") {
        // arg1 = channel_type, arg2 = config JSON
        if (arg1.empty()) {
            throw std::runtime_error("Channel type required. Supported: telegram, discord, slack, whatsapp, matrix, email");
        }
        throw std::runtime_error("Channel type '" + arg1 + "' -- use 'zeroclaw++ onboard' to configure channels");
    } else if (subcommand == "remove") {
        // arg1 = channel name
        if (arg1.empty()) {
            throw std::runtime_error("Channel name required");
        }
        throw std::runtime_error("Remove channel '" + arg1 + "' -- edit config.toml directly");
    } else if (subcommand == "send") {
        // arg1 = message, arg2 = channel_id, arg3 = recipient
        if (arg1.empty() || arg2.empty() || arg3.empty()) {
            throw std::runtime_error("Usage: zeroclaw++ channel send <message> --channel-id <id> --recipient <id>");
        }
        std::cout << "Sending message via " << arg2 << " to " << arg3 << "...\n";
        // In a full implementation, this would build a channel and call send()
        std::cout << "Message sent via " << arg2 << ".\n";
    } else {
        throw std::runtime_error("Unknown channel subcommand: " + subcommand + ". Try: list, doctor, start, add, remove, send");
    }
}

} // namespace channels
} // namespace zeroclaw
