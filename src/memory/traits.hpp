#pragma once

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <chrono>
#include <memory>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace zeroclaw::memory {

class MemoryCategory {
public:
    enum class Kind { Core, Daily, Conversation };
    
    using CustomType = std::string;
    
private:
    std::variant<Kind, CustomType> value_;
    
public:
    MemoryCategory() : value_(Kind::Core) {}
    MemoryCategory(Kind k) : value_(k) {}
    explicit MemoryCategory(const std::string& custom) : value_(custom) {}
    MemoryCategory(const MemoryCategory&) = default;
    MemoryCategory(MemoryCategory&&) = default;
    MemoryCategory& operator=(const MemoryCategory&) = default;
    MemoryCategory& operator=(MemoryCategory&&) = default;
    
    bool is_core() const { return std::holds_alternative<Kind>(value_) && std::get<Kind>(value_) == Kind::Core; }
    bool is_daily() const { return std::holds_alternative<Kind>(value_) && std::get<Kind>(value_) == Kind::Daily; }
    bool is_conversation() const { return std::holds_alternative<Kind>(value_) && std::get<Kind>(value_) == Kind::Conversation; }
    bool is_custom() const { return std::holds_alternative<CustomType>(value_); }
    
    std::string to_string() const {
        if (std::holds_alternative<Kind>(value_)) {
            switch (std::get<Kind>(value_)) {
                case Kind::Core: return "core";
                case Kind::Daily: return "daily";
                case Kind::Conversation: return "conversation";
            }
        }
        return std::get<CustomType>(value_);
    }
    
    static MemoryCategory from_string(const std::string& s) {
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(), 
                       [](unsigned char c){ return std::tolower(c); });
        if (lower == "core") return MemoryCategory(Kind::Core);
        if (lower == "daily") return MemoryCategory(Kind::Daily);
        if (lower == "conversation") return MemoryCategory(Kind::Conversation);
        return MemoryCategory(s);
    }
    
    bool operator==(const MemoryCategory& other) const {
        return value_ == other.value_;
    }
    
    bool operator!=(const MemoryCategory& other) const {
        return !(*this == other);
    }
};

struct MemoryEntry {
    std::string id;
    std::string key;
    std::string content;
    MemoryCategory category;
    std::string timestamp;
    std::optional<std::string> session_id;
    std::optional<double> score;
    
    MemoryEntry() = default;
    MemoryEntry(std::string i, std::string k, std::string c, MemoryCategory cat,
                std::string ts, std::optional<std::string> sid = std::nullopt,
                std::optional<double> sc = std::nullopt)
        : id(std::move(i)), key(std::move(k)), content(std::move(c)),
          category(std::move(cat)), timestamp(std::move(ts)),
          session_id(std::move(sid)), score(std::move(sc)) {}
};

class MemoryError : public std::runtime_error {
public:
    explicit MemoryError(const std::string& msg) : std::runtime_error(msg) {}
};

class Memory {
public:
    virtual ~Memory() = default;
    
    virtual std::string name() const = 0;
    
    virtual void store(const std::string& key,
                       const std::string& content,
                       const MemoryCategory& category,
                       const std::optional<std::string>& session_id = std::nullopt) = 0;
    
    virtual std::vector<MemoryEntry> recall(const std::string& query,
                                            size_t limit,
                                            const std::optional<std::string>& session_id = std::nullopt) = 0;
    
    virtual std::optional<MemoryEntry> get(const std::string& key) = 0;
    
    virtual std::vector<MemoryEntry> list(const std::optional<MemoryCategory>& category = std::nullopt,
                                          const std::optional<std::string>& session_id = std::nullopt) = 0;
    
    virtual bool forget(const std::string& key) = 0;
    
    virtual size_t count() = 0;
    
    virtual bool health_check() = 0;
};

inline std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

}
