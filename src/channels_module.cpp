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
    // Stub implementation
}

} // namespace channels
} // namespace zeroclaw
