#pragma once

#include "traits.hpp"
#include "embeddings.hpp"
#include "vector.hpp"
#include <filesystem>
#include <sqlite3.h>
#include <mutex>
#include <memory>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace zeroclaw::memory {

namespace detail {
    inline std::string generate_uuid() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::uniform_int_distribution<> dis2(8, 11);
        
        std::stringstream ss;
        ss << std::hex;
        for (int i = 0; i < 8; ++i) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; ++i) ss << dis(gen);
        ss << "-4";
        for (int i = 0; i < 3; ++i) ss << dis(gen);
        ss << "-";
        ss << dis2(gen);
        for (int i = 0; i < 3; ++i) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 12; ++i) ss << dis(gen);
        return ss.str();
    }
    
    inline std::string content_hash(const std::string& text) {
        std::hash<std::string> hasher;
        size_t h = hasher(text);
        std::stringstream ss;
        ss << std::hex << std::setw(16) << std::setfill('0') << h;
        return ss.str();
    }
}

class SqliteMemory : public Memory {
    std::filesystem::path db_path_;
    std::shared_ptr<sqlite3> conn_;
    std::shared_ptr<EmbeddingProvider> embedder_;
    float vector_weight_;
    float keyword_weight_;
    size_t cache_max_;
    std::mutex mutex_;
    
    static std::string category_to_str(const MemoryCategory& cat) {
        if (cat.is_core()) return "core";
        if (cat.is_daily()) return "daily";
        if (cat.is_conversation()) return "conversation";
        return cat.to_string();
    }
    
    static MemoryCategory str_to_category(const std::string& s) {
        if (s == "core") return MemoryCategory(MemoryCategory::Kind::Core);
        if (s == "daily") return MemoryCategory(MemoryCategory::Kind::Daily);
        if (s == "conversation") return MemoryCategory(MemoryCategory::Kind::Conversation);
        return MemoryCategory(s);
    }
    
    void init_schema() {
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS memories (
                id          TEXT PRIMARY KEY,
                key         TEXT NOT NULL UNIQUE,
                content     TEXT NOT NULL,
                category    TEXT NOT NULL DEFAULT 'core',
                embedding   BLOB,
                created_at  TEXT NOT NULL,
                updated_at  TEXT NOT NULL,
                session_id  TEXT
            );
            CREATE INDEX IF NOT EXISTS idx_memories_category ON memories(category);
            CREATE INDEX IF NOT EXISTS idx_memories_key ON memories(key);
            
            CREATE VIRTUAL TABLE IF NOT EXISTS memories_fts USING fts5(
                key, content, content=memories, content_rowid=rowid
            );
            
            CREATE TRIGGER IF NOT EXISTS memories_ai AFTER INSERT ON memories BEGIN
                INSERT INTO memories_fts(rowid, key, content)
                VALUES (new.rowid, new.key, new.content);
            END;
            CREATE TRIGGER IF NOT EXISTS memories_ad AFTER DELETE ON memories BEGIN
                INSERT INTO memories_fts(memories_fts, rowid, key, content)
                VALUES ('delete', old.rowid, old.key, old.content);
            END;
            CREATE TRIGGER IF NOT EXISTS memories_au AFTER UPDATE ON memories BEGIN
                INSERT INTO memories_fts(memories_fts, rowid, key, content)
                VALUES ('delete', old.rowid, old.key, old.content);
                INSERT INTO memories_fts(rowid, key, content)
                VALUES (new.rowid, new.key, new.content);
            END;
            
            CREATE TABLE IF NOT EXISTS embedding_cache (
                content_hash TEXT PRIMARY KEY,
                embedding    BLOB NOT NULL,
                created_at   TEXT NOT NULL,
                accessed_at  TEXT NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_cache_accessed ON embedding_cache(accessed_at);
        )";
        
        char* err = nullptr;
        int rc = sqlite3_exec(conn_.get(), sql, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::string msg = err ? err : "schema init failed";
            sqlite3_free(err);
            throw MemoryError(msg);
        }
    }
    
    std::optional<std::vector<float>> get_or_compute_embedding(const std::string& text) {
        if (embedder_->dimensions() == 0) {
            return std::nullopt;
        }
        
        std::string hash = detail::content_hash(text);
        std::string now = current_timestamp();
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            sqlite3_stmt* stmt;
            const char* sql = "SELECT embedding FROM embedding_cache WHERE content_hash = ?";
            if (sqlite3_prepare_v2(conn_.get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const void* blob = sqlite3_column_blob(stmt, 0);
                    int size = sqlite3_column_bytes(stmt, 0);
                    std::vector<uint8_t> bytes(static_cast<const uint8_t*>(blob),
                                               static_cast<const uint8_t*>(blob) + size);
                    sqlite3_finalize(stmt);
                    
                    const char* update_sql = "UPDATE embedding_cache SET accessed_at = ? WHERE content_hash = ?";
                    if (sqlite3_prepare_v2(conn_.get(), update_sql, -1, &stmt, nullptr) == SQLITE_OK) {
                        sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_step(stmt);
                        sqlite3_finalize(stmt);
                    }
                    
                    return bytes_to_vec(bytes);
                }
                sqlite3_finalize(stmt);
            }
        }
        
        std::vector<float> embedding = embedder_->embed_one(text);
        std::vector<uint8_t> bytes = vec_to_bytes(embedding);
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            sqlite3_stmt* stmt;
            const char* sql = "INSERT OR REPLACE INTO embedding_cache (content_hash, embedding, created_at, accessed_at) VALUES (?, ?, ?, ?)";
            if (sqlite3_prepare_v2(conn_.get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_blob(stmt, 2, bytes.data(), bytes.size(), SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }
        
        return embedding;
    }
    
public:
    SqliteMemory(const std::filesystem::path& workspace_dir,
                 std::shared_ptr<EmbeddingProvider> embedder = std::make_shared<NoopEmbedding>(),
                 float vector_weight = 0.7f,
                 float keyword_weight = 0.3f,
                 size_t cache_max = 10000)
        : embedder_(std::move(embedder))
        , vector_weight_(vector_weight)
        , keyword_weight_(keyword_weight)
        , cache_max_(cache_max)
    {
        db_path_ = workspace_dir / "memory" / "brain.db";
        std::filesystem::create_directories(db_path_.parent_path());
        
        sqlite3* raw_conn = nullptr;
        int rc = sqlite3_open(db_path_.string().c_str(), &raw_conn);
        if (rc != SQLITE_OK) {
            throw MemoryError("Failed to open SQLite database");
        }
        conn_.reset(raw_conn, sqlite3_close);
        
        const char* pragma = "PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL;";
        char* err = nullptr;
        sqlite3_exec(conn_.get(), pragma, nullptr, nullptr, &err);
        sqlite3_free(err);
        
        init_schema();
    }
    
    std::string name() const override { return "sqlite"; }
    
    void store(const std::string& key,
               const std::string& content,
               const MemoryCategory& category,
               const std::optional<std::string>& session_id = std::nullopt) override
    {
        auto maybe_embedding = get_or_compute_embedding(content);
        std::vector<uint8_t> embedding_bytes;
        if (maybe_embedding) {
            embedding_bytes = vec_to_bytes(*maybe_embedding);
        }
        
        std::string now = current_timestamp();
        std::string cat = category_to_str(category);
        std::string id = detail::generate_uuid();
        
        std::lock_guard<std::mutex> lock(mutex_);
        const char* sql = R"(
            INSERT INTO memories (id, key, content, category, embedding, created_at, updated_at, session_id)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(key) DO UPDATE SET
                content = excluded.content,
                category = excluded.category,
                embedding = excluded.embedding,
                updated_at = excluded.updated_at,
                session_id = excluded.session_id
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(conn_.get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, cat.c_str(), -1, SQLITE_TRANSIENT);
            if (embedding_bytes.empty()) {
                sqlite3_bind_null(stmt, 5);
            } else {
                sqlite3_bind_blob(stmt, 5, embedding_bytes.data(), embedding_bytes.size(), SQLITE_TRANSIENT);
            }
            sqlite3_bind_text(stmt, 6, now.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);
            if (session_id) {
                sqlite3_bind_text(stmt, 8, session_id->c_str(), -1, SQLITE_TRANSIENT);
            } else {
                sqlite3_bind_null(stmt, 8);
            }
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    
    std::vector<MemoryEntry> recall(const std::string& query,
                                    size_t limit,
                                    const std::optional<std::string>& session_id = std::nullopt) override
    {
        if (query.empty() || query.find_first_not_of(" \t\r\n") == std::string::npos) {
            return {};
        }
        
        auto maybe_query_embedding = get_or_compute_embedding(query);
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::pair<std::string, float>> keyword_results;
        {
            std::string fts_query;
            std::istringstream qs(query);
            std::string word;
            while (qs >> word) {
                if (!fts_query.empty()) fts_query += " OR ";
                fts_query += "\"" + word + "\"";
            }
            
            std::string sql = "SELECT m.id, bm25(memories_fts) FROM memories_fts f "
                             "JOIN memories m ON m.rowid = f.rowid "
                             "WHERE memories_fts MATCH ? ORDER BY bm25(memories_fts) LIMIT ?";
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(conn_.get(), sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, fts_query.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(limit * 2));
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    std::string id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    float score = static_cast<float>(-sqlite3_column_double(stmt, 1));
                    keyword_results.emplace_back(id, score);
                }
                sqlite3_finalize(stmt);
            }
        }
        
        std::vector<std::pair<std::string, float>> vector_results;
        if (maybe_query_embedding) {
            std::string sql = "SELECT id, embedding FROM memories WHERE embedding IS NOT NULL";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(conn_.get(), sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    std::string id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    const void* blob = sqlite3_column_blob(stmt, 1);
                    int size = sqlite3_column_bytes(stmt, 1);
                    std::vector<uint8_t> bytes(static_cast<const uint8_t*>(blob),
                                               static_cast<const uint8_t*>(blob) + size);
                    auto emb = bytes_to_vec(bytes);
                    float sim = cosine_similarity(*maybe_query_embedding, emb);
                    if (sim > 0.0f) {
                        vector_results.emplace_back(id, sim);
                    }
                }
                sqlite3_finalize(stmt);
            }
        }
        
        auto merged = hybrid_merge(vector_results, keyword_results, vector_weight_, keyword_weight_, limit);
        
        std::vector<MemoryEntry> results;
        for (const auto& s : merged) {
            const char* sql = "SELECT id, key, content, category, created_at, session_id FROM memories WHERE id = ?";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(conn_.get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, s.id.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    MemoryEntry entry;
                    entry.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    entry.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    entry.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                    entry.category = str_to_category(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
                    entry.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                    if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
                        entry.session_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                    }
                    entry.score = s.final_score;
                    
                    if (!session_id || entry.session_id == session_id) {
                        results.push_back(entry);
                    }
                }
                sqlite3_finalize(stmt);
            }
        }
        
        if (results.size() > limit) {
            results.resize(limit);
        }
        
        return results;
    }
    
    std::optional<MemoryEntry> get(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const char* sql = "SELECT id, key, content, category, created_at, session_id FROM memories WHERE key = ?";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(conn_.get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                MemoryEntry entry;
                entry.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                entry.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                entry.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                entry.category = str_to_category(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
                entry.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
                    entry.session_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                }
                sqlite3_finalize(stmt);
                return entry;
            }
            sqlite3_finalize(stmt);
        }
        return std::nullopt;
    }
    
    std::vector<MemoryEntry> list(const std::optional<MemoryCategory>& category = std::nullopt,
                                  const std::optional<std::string>& session_id = std::nullopt) override
    {
        std::vector<MemoryEntry> results;
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string sql = "SELECT id, key, content, category, created_at, session_id FROM memories";
        if (category) {
            sql += " WHERE category = ?";
        }
        sql += " ORDER BY updated_at DESC LIMIT 1000";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(conn_.get(), sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            if (category) {
                std::string cat = category_to_str(*category);
                sqlite3_bind_text(stmt, 1, cat.c_str(), -1, SQLITE_TRANSIENT);
            }
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                MemoryEntry entry;
                entry.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                entry.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                entry.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                entry.category = str_to_category(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
                entry.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
                    entry.session_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                }
                if (!session_id || entry.session_id == session_id) {
                    results.push_back(entry);
                }
            }
            sqlite3_finalize(stmt);
        }
        
        return results;
    }
    
    bool forget(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const char* sql = "DELETE FROM memories WHERE key = ?";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(conn_.get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            int changes = sqlite3_changes(conn_.get());
            sqlite3_finalize(stmt);
            return changes > 0;
        }
        return false;
    }
    
    size_t count() override {
        std::lock_guard<std::mutex> lock(mutex_);
        const char* sql = "SELECT COUNT(*) FROM memories";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(conn_.get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                size_t c = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
                sqlite3_finalize(stmt);
                return c;
            }
            sqlite3_finalize(stmt);
        }
        return 0;
    }
    
    bool health_check() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return sqlite3_exec(conn_.get(), "SELECT 1", nullptr, nullptr, nullptr) == SQLITE_OK;
    }
};

}
