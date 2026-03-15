#pragma once

#include "traits.hpp"
#include "sqlite.hpp"
#include <filesystem>
#include <memory>
#include <mutex>
#include <chrono>

namespace zeroclaw::memory {

class LucidMemory : public Memory {
    std::shared_ptr<SqliteMemory> local_;
    std::string lucid_cmd_;
    size_t token_budget_;
    std::filesystem::path workspace_dir_;
    std::chrono::milliseconds recall_timeout_;
    std::chrono::milliseconds store_timeout_;
    size_t local_hit_threshold_;
    std::chrono::milliseconds failure_cooldown_;
    std::mutex mutex_;
    std::optional<std::chrono::steady_clock::time_point> last_failure_at_;
    
    static std::string to_lucid_type(const MemoryCategory& category) {
        if (category.is_core()) return "decision";
        if (category.is_daily()) return "context";
        if (category.is_conversation()) return "conversation";
        return "learning";
    }
    
    static MemoryCategory to_memory_category(const std::string& label) {
        std::string lower = label;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        
        if (lower.find("visual") != std::string::npos) {
            return MemoryCategory("visual");
        }
        
        if (lower == "decision" || lower == "learning" || lower == "solution") {
            return MemoryCategory(MemoryCategory::Kind::Core);
        }
        if (lower == "context" || lower == "conversation") {
            return MemoryCategory(MemoryCategory::Kind::Conversation);
        }
        if (lower == "bug") {
            return MemoryCategory(MemoryCategory::Kind::Daily);
        }
        return MemoryCategory(label);
    }
    
    bool in_failure_cooldown() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (last_failure_at_) {
            auto elapsed = std::chrono::steady_clock::now() - *last_failure_at_;
            return elapsed < failure_cooldown_;
        }
        return false;
    }
    
    void mark_failure() {
        std::lock_guard<std::mutex> lock(mutex_);
        last_failure_at_ = std::chrono::steady_clock::now();
    }
    
    void clear_failure() {
        std::lock_guard<std::mutex> lock(mutex_);
        last_failure_at_ = std::nullopt;
    }
    
public:
    LucidMemory(const std::filesystem::path& workspace_dir,
                std::shared_ptr<SqliteMemory> local,
                std::string lucid_cmd = "lucid",
                size_t token_budget = 200,
                size_t local_hit_threshold = 3,
                std::chrono::milliseconds recall_timeout = std::chrono::milliseconds(500),
                std::chrono::milliseconds store_timeout = std::chrono::milliseconds(800),
                std::chrono::milliseconds failure_cooldown = std::chrono::milliseconds(15000))
        : local_(std::move(local))
        , lucid_cmd_(std::move(lucid_cmd))
        , token_budget_(token_budget)
        , workspace_dir_(workspace_dir)
        , recall_timeout_(recall_timeout)
        , store_timeout_(store_timeout)
        , local_hit_threshold_(local_hit_threshold)
        , failure_cooldown_(failure_cooldown)
    {}
    
    std::string name() const override { return "lucid"; }
    
    void store(const std::string& key,
               const std::string& content,
               const MemoryCategory& category,
               const std::optional<std::string>& session_id = std::nullopt) override
    {
        local_->store(key, content, category, session_id);
    }
    
    std::vector<MemoryEntry> recall(const std::string& query,
                                    size_t limit,
                                    const std::optional<std::string>& session_id = std::nullopt) override
    {
        auto local_results = local_->recall(query, limit, session_id);
        if (limit == 0 || local_results.size() >= limit || local_results.size() >= local_hit_threshold_) {
            return local_results;
        }
        
        if (in_failure_cooldown()) {
            return local_results;
        }
        
        return local_results;
    }
    
    std::optional<MemoryEntry> get(const std::string& key) override {
        return local_->get(key);
    }
    
    std::vector<MemoryEntry> list(const std::optional<MemoryCategory>& category = std::nullopt,
                                  const std::optional<std::string>& session_id = std::nullopt) override
    {
        return local_->list(category, session_id);
    }
    
    bool forget(const std::string& key) override {
        return local_->forget(key);
    }
    
    size_t count() override {
        return local_->count();
    }
    
    bool health_check() override {
        return local_->health_check();
    }
};

}
