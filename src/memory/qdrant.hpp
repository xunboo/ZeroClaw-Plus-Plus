#pragma once
/// Qdrant vector database memory backend.
/// Mirrors Rust src/memory/qdrant.rs

#include "traits.hpp"
#include "embeddings.hpp"
#include "../http/http_client.hpp"
#include <string>
#include <optional>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace zeroclaw {
namespace memory {

/// Qdrant vector database memory backend.
/// Uses Qdrant's REST API for vector storage and semantic search.
/// Requires an embedding provider for converting text to vectors.
class QdrantMemory : public Memory {
public:
    /// Create a Qdrant memory backend with lazy collection initialization.
    QdrantMemory(const std::string& url,
                 const std::string& collection,
                 std::optional<std::string> api_key,
                 std::shared_ptr<EmbeddingProvider> embedder);

    std::string name() const override { return "qdrant"; }

    // Memory interface — matching Rust impl
    bool store(const std::string& key,
               const std::string& content,
               MemoryCategory category,
               std::optional<std::string> session_id) override;

    std::vector<MemoryEntry> recall(const std::string& query,
                                     size_t limit,
                                     std::optional<std::string> session_id) override;

    std::optional<MemoryEntry> get(const std::string& key) override;

    std::vector<MemoryEntry> list(std::optional<MemoryCategory> category,
                                   std::optional<std::string> session_id) override;

    bool forget(const std::string& key) override;

    size_t count() override;

    bool health_check() override;

private:
    /// Ensure the Qdrant collection exists with the right vector config (lazy).
    bool ensure_collection();

    /// Build a category string matching Rust's category_to_str()
    static std::string category_to_str(MemoryCategory cat);

    /// Parse a category string matching Rust's parse_category()
    static MemoryCategory parse_category(const std::string& s);

    /// Delete all points with matching key (used before upsert to avoid duplicates)
    bool delete_by_key(const std::string& key);

    /// Parse a MemoryEntry from Qdrant point JSON
    static std::optional<MemoryEntry> parse_point(const nlohmann::json& point);

    std::string base_url_;
    std::string collection_;
    std::optional<std::string> api_key_;
    std::shared_ptr<EmbeddingProvider> embedder_;

    mutable std::mutex init_mutex_;
    std::atomic<bool> initialized_{false};

    /// Add auth header to request builder
    http::HttpClient make_client() const;
};

} // namespace memory
} // namespace zeroclaw
