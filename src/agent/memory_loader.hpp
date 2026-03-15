#pragma once

/// Memory loader — loads relevant memory context for each user message.

#include <string>
#include <vector>
#include <optional>
#include <sstream>

namespace zeroclaw {

// Forward declarations for memory types
struct MemoryEntry {
    std::string id;
    std::string key;
    std::string content;
    std::string category;
    std::string timestamp;
    std::optional<std::string> session_id;
    std::optional<double> score;
};

/// Abstract memory interface
class Memory {
public:
    virtual ~Memory() = default;
    virtual bool store(const std::string& key, const std::string& content,
                       const std::string& category, const std::optional<std::string>& session_id) = 0;
    virtual std::vector<MemoryEntry> recall(const std::string& query, size_t limit,
                                             const std::optional<std::string>& session_id) = 0;
    virtual std::optional<MemoryEntry> get(const std::string& key) = 0;
    virtual std::vector<MemoryEntry> list(const std::optional<std::string>& category,
                                           const std::optional<std::string>& session_id) = 0;
    virtual bool forget(const std::string& key) = 0;
    virtual size_t count() = 0;
    virtual bool health_check() const = 0;
    virtual std::string name() const = 0;
};

/// Check if a memory key is an auto-saved assistant entry
inline bool is_assistant_autosave_key(const std::string& key) {
    return key.find("assistant_resp") == 0;
}

namespace agent {

/// Abstract memory loader interface
class MemoryLoader {
public:
    virtual ~MemoryLoader() = default;
    virtual std::string load_context(Memory* memory, const std::string& user_message) = 0;
};

/// Default memory loader implementation
class DefaultMemoryLoader : public MemoryLoader {
public:
    DefaultMemoryLoader() = default;
    DefaultMemoryLoader(size_t limit, double min_relevance_score)
        : limit_(std::max(limit, size_t(1))), min_relevance_score_(min_relevance_score) {}

    std::string load_context(Memory* memory, const std::string& user_message) override {
        auto entries = memory->recall(user_message, limit_, std::nullopt);
        if (entries.empty()) return "";

        std::ostringstream oss;
        oss << "[Memory context]\n";
        bool has_entry = false;
        for (const auto& entry : entries) {
            if (is_assistant_autosave_key(entry.key)) continue;
            if (entry.score.has_value() && *entry.score < min_relevance_score_) continue;
            oss << "- " << entry.key << ": " << entry.content << "\n";
            has_entry = true;
        }

        if (!has_entry) return "";

        oss << "\n";
        return oss.str();
    }

private:
    size_t limit_ = 5;
    double min_relevance_score_ = 0.4;
};

} // namespace agent
} // namespace zeroclaw
