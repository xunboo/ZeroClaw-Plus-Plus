#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <algorithm>
#include <cctype>

namespace zeroclaw {
namespace config {
struct Config;
}

namespace integrations {

enum class IntegrationStatus {
    Available,
    Active,
    ComingSoon
};

enum class IntegrationCategory {
    Chat,
    AiModel,
    Productivity,
    MusicAudio,
    SmartHome,
    ToolsAutomation,
    MediaCreative,
    Social,
    Platform
};

inline const char* label(IntegrationCategory cat) {
    switch (cat) {
        case IntegrationCategory::Chat: return "Chat Providers";
        case IntegrationCategory::AiModel: return "AI Models";
        case IntegrationCategory::Productivity: return "Productivity";
        case IntegrationCategory::MusicAudio: return "Music & Audio";
        case IntegrationCategory::SmartHome: return "Smart Home";
        case IntegrationCategory::ToolsAutomation: return "Tools & Automation";
        case IntegrationCategory::MediaCreative: return "Media & Creative";
        case IntegrationCategory::Social: return "Social";
        case IntegrationCategory::Platform: return "Platforms";
    }
    return "";
}

inline const std::vector<IntegrationCategory>& all_categories() {
    static const std::vector<IntegrationCategory> categories = {
        IntegrationCategory::Chat,
        IntegrationCategory::AiModel,
        IntegrationCategory::Productivity,
        IntegrationCategory::MusicAudio,
        IntegrationCategory::SmartHome,
        IntegrationCategory::ToolsAutomation,
        IntegrationCategory::MediaCreative,
        IntegrationCategory::Social,
        IntegrationCategory::Platform
    };
    return categories;
}

struct IntegrationEntry {
    const char* name;
    const char* description;
    IntegrationCategory category;
    std::function<IntegrationStatus(const config::Config&)> status_fn;
};

std::vector<IntegrationEntry> all_integrations();

std::string to_lower(const std::string& s);

void show_integration_info(const config::Config& config, const std::string& name);

}
}
