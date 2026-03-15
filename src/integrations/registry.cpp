#include "integration.hpp"
#include "../config/config.hpp"
#include <string>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <cstdio>

namespace zeroclaw {
namespace integrations {
namespace providers {
    bool is_glm_alias(const std::string& s) {
        return s == "glm" || s == "glm-cn" || s == "chatglm" || s == "zhipu";
    }
    bool is_minimax_alias(const std::string& s) {
        return s == "minimax" || s == "minimax-cn" || s == "minimax-intl";
    }
    bool is_moonshot_alias(const std::string& s) {
        return s == "moonshot" || s == "moonshot-cn" || s == "moonshot-intl" || s == "kimi";
    }
    bool is_qianfan_alias(const std::string& s) {
        return s == "qianfan" || s == "baidu" || s == "ernie";
    }
    bool is_qwen_alias(const std::string& s) {
        return s == "qwen" || s == "qwen-cn" || s == "qwen-intl" || s == "dashscope";
    }
    bool is_zai_alias(const std::string& s) {
        return s == "zai" || s == "zai-cn" || s == "z.ai";
    }
}

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::vector<IntegrationEntry> all_integrations() {
    using namespace zeroclaw::config;
    return {
        { "Telegram", "Bot API — long-polling", IntegrationCategory::Chat,
          [](const Config& c) { return c.channels_config.telegram.has_value() ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Discord", "Servers, channels & DMs", IntegrationCategory::Chat,
          [](const Config& c) { return c.channels_config.discord.has_value() ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Slack", "Workspace apps via Web API", IntegrationCategory::Chat,
          [](const Config& c) { return c.channels_config.slack.has_value() ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Webhooks", "HTTP endpoint for triggers", IntegrationCategory::Chat,
          [](const Config& c) { return c.channels_config.webhook.has_value() ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "WhatsApp", "Meta Cloud API via webhook", IntegrationCategory::Chat,
          [](const Config& c) { return c.channels_config.whatsapp.has_value() ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Signal", "Privacy-focused via signal-cli", IntegrationCategory::Chat,
          [](const Config& c) { return c.channels_config.signal.has_value() ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "iMessage", "macOS AppleScript bridge", IntegrationCategory::Chat,
          [](const Config& c) { return c.channels_config.imessage.has_value() ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Microsoft Teams", "Enterprise chat support", IntegrationCategory::Chat,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Matrix", "Matrix protocol (Element)", IntegrationCategory::Chat,
          [](const Config& c) { return c.channels_config.matrix.has_value() ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Nostr", "Decentralized DMs (NIP-04)", IntegrationCategory::Chat,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "WebChat", "Browser-based chat UI", IntegrationCategory::Chat,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Nextcloud Talk", "Self-hosted Nextcloud chat", IntegrationCategory::Chat,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Zalo", "Zalo Bot API", IntegrationCategory::Chat,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "DingTalk", "DingTalk Stream Mode", IntegrationCategory::Chat,
          [](const Config& c) { return c.channels_config.dingtalk.has_value() ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "QQ Official", "Tencent QQ Bot SDK", IntegrationCategory::Chat,
          [](const Config& c) { return c.channels_config.qq.has_value() ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "OpenRouter", "200+ models, 1 API key", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "openrouter" && c.api_key.has_value()) ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Anthropic", "Claude 3.5/4 Sonnet & Opus", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "anthropic") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "OpenAI", "GPT-4o, GPT-5, o1", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "openai") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Google", "Gemini 2.5 Pro/Flash", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_model.has_value() && c.default_model.value().rfind("google/", 0) == 0) ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "DeepSeek", "DeepSeek V3 & R1", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_model.has_value() && c.default_model.value().rfind("deepseek/", 0) == 0) ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "xAI", "Grok 3 & 4", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_model.has_value() && c.default_model.value().rfind("x-ai/", 0) == 0) ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Mistral", "Mistral Large & Codestral", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_model.has_value() && c.default_model.value().rfind("mistral", 0) == 0) ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Ollama", "Local models (Llama, etc.)", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "ollama") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Perplexity", "Search-augmented AI", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "perplexity") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Hugging Face", "Open-source models", IntegrationCategory::AiModel,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "LM Studio", "Local model server", IntegrationCategory::AiModel,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Venice", "Privacy-first inference (Llama, Opus)", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "venice") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Vercel AI", "Vercel AI Gateway", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "vercel") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Cloudflare AI", "Cloudflare AI Gateway", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "cloudflare") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Moonshot", "Kimi & Kimi Coding", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && providers::is_moonshot_alias(c.default_provider.value())) ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Synthetic", "Synthetic AI models", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "synthetic") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "OpenCode Zen", "Code-focused AI models", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "opencode") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Z.AI", "Z.AI inference", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && providers::is_zai_alias(c.default_provider.value())) ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "GLM", "ChatGLM / Zhipu models", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && providers::is_glm_alias(c.default_provider.value())) ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "MiniMax", "MiniMax AI models", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && providers::is_minimax_alias(c.default_provider.value())) ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Qwen", "Alibaba DashScope Qwen models", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && providers::is_qwen_alias(c.default_provider.value())) ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Amazon Bedrock", "AWS managed model access", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "bedrock") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Qianfan", "Baidu AI models", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && providers::is_qianfan_alias(c.default_provider.value())) ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Groq", "Ultra-fast LPU inference", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "groq") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Together AI", "Open-source model hosting", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "together") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Fireworks AI", "Fast open-source inference", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "fireworks") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "Cohere", "Command R+ & embeddings", IntegrationCategory::AiModel,
          [](const Config& c) { return (c.default_provider.has_value() && c.default_provider.value() == "cohere") ? IntegrationStatus::Active : IntegrationStatus::Available; } },
        { "GitHub", "Code, issues, PRs", IntegrationCategory::Productivity,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Notion", "Workspace & databases", IntegrationCategory::Productivity,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Apple Notes", "Native macOS/iOS notes", IntegrationCategory::Productivity,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Apple Reminders", "Task management", IntegrationCategory::Productivity,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Obsidian", "Knowledge graph notes", IntegrationCategory::Productivity,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Things 3", "GTD task manager", IntegrationCategory::Productivity,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Bear Notes", "Markdown notes", IntegrationCategory::Productivity,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Trello", "Kanban boards", IntegrationCategory::Productivity,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Linear", "Issue tracking", IntegrationCategory::Productivity,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Spotify", "Music playback control", IntegrationCategory::MusicAudio,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Sonos", "Multi-room audio", IntegrationCategory::MusicAudio,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Shazam", "Song recognition", IntegrationCategory::MusicAudio,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Home Assistant", "Home automation hub", IntegrationCategory::SmartHome,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Philips Hue", "Smart lighting", IntegrationCategory::SmartHome,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "8Sleep", "Smart mattress", IntegrationCategory::SmartHome,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Browser", "Chrome/Chromium control", IntegrationCategory::ToolsAutomation,
          [](const Config&) { return IntegrationStatus::Available; } },
        { "Shell", "Terminal command execution", IntegrationCategory::ToolsAutomation,
          [](const Config&) { return IntegrationStatus::Active; } },
        { "File System", "Read/write files", IntegrationCategory::ToolsAutomation,
          [](const Config&) { return IntegrationStatus::Active; } },
        { "Cron", "Scheduled tasks", IntegrationCategory::ToolsAutomation,
          [](const Config&) { return IntegrationStatus::Available; } },
        { "Voice", "Voice wake + talk mode", IntegrationCategory::ToolsAutomation,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Gmail", "Email triggers & send", IntegrationCategory::ToolsAutomation,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "1Password", "Secure credentials", IntegrationCategory::ToolsAutomation,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Weather", "Forecasts & conditions", IntegrationCategory::ToolsAutomation,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Canvas", "Visual workspace + A2UI", IntegrationCategory::ToolsAutomation,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Image Gen", "AI image generation", IntegrationCategory::MediaCreative,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "GIF Search", "Find the perfect GIF", IntegrationCategory::MediaCreative,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Screen Capture", "Screenshot & screen control", IntegrationCategory::MediaCreative,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Camera", "Photo/video capture", IntegrationCategory::MediaCreative,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Twitter/X", "Tweet, reply, search", IntegrationCategory::Social,
          [](const Config&) { return IntegrationStatus::ComingSoon; } },
        { "Email", "IMAP/SMTP email channel", IntegrationCategory::Social,
          [](const Config&) { return IntegrationStatus::Available; } },
        { "macOS", "Native support + AppleScript", IntegrationCategory::Platform,
          [](const Config&) {
#ifdef __APPLE__
              return IntegrationStatus::Active;
#else
              return IntegrationStatus::Available;
#endif
          } },
        { "Linux", "Native support", IntegrationCategory::Platform,
          [](const Config&) {
#ifdef __linux__
              return IntegrationStatus::Active;
#else
              return IntegrationStatus::Available;
#endif
          } },
        { "Windows", "WSL2 recommended", IntegrationCategory::Platform,
          [](const Config&) { return IntegrationStatus::Available; } },
        { "iOS", "Chat via Telegram/Discord", IntegrationCategory::Platform,
          [](const Config&) { return IntegrationStatus::Available; } },
        { "Android", "Chat via Telegram/Discord", IntegrationCategory::Platform,
          [](const Config&) { return IntegrationStatus::Available; } },
    };
}

void show_integration_info(const config::Config& config, const std::string& name) {
    auto entries = all_integrations();
    std::string name_lower = to_lower(name);

    auto it = std::find_if(entries.begin(), entries.end(),
        [&name_lower](const IntegrationEntry& e) {
            return to_lower(e.name) == name_lower;
        });

    if (it == entries.end()) {
        throw std::runtime_error("Unknown integration: " + name);
    }

    const auto& entry = *it;
    auto status = entry.status_fn(config);

    const char* icon;
    const char* status_label;
    switch (status) {
        case IntegrationStatus::Active:
            icon = "[*]";
            status_label = "Active";
            break;
        case IntegrationStatus::Available:
            icon = "[ ]";
            status_label = "Available";
            break;
        case IntegrationStatus::ComingSoon:
        default:
            icon = "[?]";
            status_label = "Coming Soon";
            break;
    }

    std::printf("\n  %s %s — %s\n", icon, entry.name, entry.description);
    std::printf("  Category: %s\n", label(entry.category));
    std::printf("  Status:   %s\n\n", status_label);
}

}
}
