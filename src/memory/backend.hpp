#pragma once

#include <string>
#include <array>
#include <string_view>
#include <algorithm>
#include <cctype>

namespace zeroclaw::memory {

enum class MemoryBackendKind {
    Sqlite,
    Lucid,
    Postgres,
    Markdown,
    None,
    Unknown
};

struct MemoryBackendProfile {
    std::string_view key;
    std::string_view label;
    bool auto_save_default;
    bool uses_sqlite_hygiene;
    bool sqlite_based;
    bool optional_dependency;
};

inline constexpr MemoryBackendProfile SQLITE_PROFILE{
    "sqlite",
    "SQLite with Vector Search (recommended) — fast, hybrid search, embeddings",
    true, true, true, false
};

inline constexpr MemoryBackendProfile LUCID_PROFILE{
    "lucid",
    "Lucid Memory bridge — sync with local lucid-memory CLI, keep SQLite fallback",
    true, true, true, true
};

inline constexpr MemoryBackendProfile MARKDOWN_PROFILE{
    "markdown",
    "Markdown Files — simple, human-readable, no dependencies",
    true, false, false, false
};

inline constexpr MemoryBackendProfile POSTGRES_PROFILE{
    "postgres",
    "PostgreSQL — remote durable storage via [storage.provider.config]",
    true, false, false, true
};

inline constexpr MemoryBackendProfile NONE_PROFILE{
    "none",
    "None — disable persistent memory",
    false, false, false, false
};

inline constexpr MemoryBackendProfile CUSTOM_PROFILE{
    "custom",
    "Custom backend — extension point",
    true, false, false, false
};

inline constexpr std::array<MemoryBackendProfile, 4> SELECTABLE_MEMORY_BACKENDS{
    SQLITE_PROFILE,
    LUCID_PROFILE,
    MARKDOWN_PROFILE,
    NONE_PROFILE
};

inline std::string_view default_memory_backend_key() {
    return SQLITE_PROFILE.key;
}

inline MemoryBackendKind classify_memory_backend(const std::string& backend) {
    std::string lower = backend;
    std::transform(lower.begin(), lower.end(), lower.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
    if (lower == "sqlite") return MemoryBackendKind::Sqlite;
    if (lower == "lucid") return MemoryBackendKind::Lucid;
    if (lower == "postgres") return MemoryBackendKind::Postgres;
    if (lower == "markdown") return MemoryBackendKind::Markdown;
    if (lower == "none") return MemoryBackendKind::None;
    return MemoryBackendKind::Unknown;
}

inline MemoryBackendProfile memory_backend_profile(const std::string& backend) {
    switch (classify_memory_backend(backend)) {
        case MemoryBackendKind::Sqlite: return SQLITE_PROFILE;
        case MemoryBackendKind::Lucid: return LUCID_PROFILE;
        case MemoryBackendKind::Postgres: return POSTGRES_PROFILE;
        case MemoryBackendKind::Markdown: return MARKDOWN_PROFILE;
        case MemoryBackendKind::None: return NONE_PROFILE;
        case MemoryBackendKind::Unknown: return CUSTOM_PROFILE;
    }
    return CUSTOM_PROFILE;
}

inline std::string effective_memory_backend_name(
    const std::string& memory_backend,
    const std::optional<std::string>& storage_provider = std::nullopt)
{
    if (storage_provider.has_value() && !storage_provider->empty()) {
        std::string trimmed = *storage_provider;
        size_t start = trimmed.find_first_not_of(" \t");
        size_t end = trimmed.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            std::string result = trimmed.substr(start, end - start + 1);
            std::transform(result.begin(), result.end(), result.begin(), 
                           [](unsigned char c){ return std::tolower(c); });
            return result;
        }
    }
    
    std::string trimmed = memory_backend;
    size_t start = trimmed.find_first_not_of(" \t");
    size_t end = trimmed.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
        trimmed = trimmed.substr(start, end - start + 1);
    }
    std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
    return trimmed;
}

inline bool is_assistant_autosave_key(const std::string& key) {
    std::string normalized = key;
    size_t start = normalized.find_first_not_of(" \t");
    size_t end = normalized.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
        normalized = normalized.substr(start, end - start + 1);
    }
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
    return normalized == "assistant_resp" || 
           (normalized.size() > 14 && normalized.substr(0, 14) == "assistant_resp_");
}

}
