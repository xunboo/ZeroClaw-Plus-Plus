#pragma once

#include "traits.hpp"
#include "backend.hpp"
#include "sqlite.hpp"
#include "markdown.hpp"
#include "none.hpp"
#include "lucid.hpp"
#include "embeddings.hpp"
#include "response_cache.hpp"
#include "hygiene.hpp"
#include "snapshot.hpp"
#include "chunker.hpp"
#include "vector.hpp"
#include <filesystem>
#include <memory>
#include <optional>

namespace zeroclaw::memory {

inline std::unique_ptr<Memory> create_memory(
    const std::string& backend_name,
    const std::filesystem::path& workspace_dir,
    std::shared_ptr<EmbeddingProvider> embedder = std::make_shared<NoopEmbedding>(),
    float vector_weight = 0.7f,
    float keyword_weight = 0.3f,
    size_t cache_max = 10000)
{
    auto kind = classify_memory_backend(backend_name);
    
    switch (kind) {
        case MemoryBackendKind::Sqlite:
            return std::make_unique<SqliteMemory>(workspace_dir, embedder, vector_weight, keyword_weight, cache_max);
        
        case MemoryBackendKind::Lucid: {
            auto local = std::make_shared<SqliteMemory>(workspace_dir, embedder, vector_weight, keyword_weight, cache_max);
            return std::make_unique<LucidMemory>(workspace_dir, local);
        }
        
        case MemoryBackendKind::Markdown:
            return std::make_unique<MarkdownMemory>(workspace_dir);
        
        case MemoryBackendKind::None:
            return std::make_unique<NoneMemory>();
        
        case MemoryBackendKind::Postgres:
            throw MemoryError("PostgreSQL backend requires libpq integration");
        
        case MemoryBackendKind::Unknown:
            return std::make_unique<MarkdownMemory>(workspace_dir);
    }
    
    return std::make_unique<MarkdownMemory>(workspace_dir);
}

inline std::unique_ptr<Memory> create_memory_for_migration(
    const std::string& backend,
    const std::filesystem::path& workspace_dir)
{
    auto kind = classify_memory_backend(backend);
    
    if (kind == MemoryBackendKind::None) {
        throw MemoryError("memory backend 'none' disables persistence; choose sqlite, lucid, or markdown before migration");
    }
    
    if (kind == MemoryBackendKind::Postgres) {
        throw MemoryError("memory migration for backend 'postgres' is unsupported; migrate with sqlite or markdown first");
    }
    
    return create_memory(backend, workspace_dir);
}

inline std::optional<ResponseCache> create_response_cache(
    const std::filesystem::path& workspace_dir,
    bool enabled,
    uint32_t ttl_minutes,
    size_t max_entries)
{
    if (!enabled) return std::nullopt;
    
    try {
        return ResponseCache(workspace_dir, ttl_minutes, max_entries);
    } catch (...) {
        return std::nullopt;
    }
}

}
