#pragma once

#include "traits.hpp"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sqlite3.h>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace zeroclaw::memory {

class ResponseCache {
    std::shared_ptr<sqlite3> conn_;
    std::filesystem::path db_path_;
    int64_t ttl_minutes_;
    size_t max_entries_;
    std::mutex mutex_;
    
    static std::string current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%S");
        return ss.str();
    }
    
    void init_schema() {
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS response_cache (
                prompt_hash TEXT PRIMARY KEY,
                model       TEXT NOT NULL,
                response    TEXT NOT NULL,
                token_count INTEGER NOT NULL DEFAULT 0,
                created_at  TEXT NOT NULL,
                accessed_at TEXT NOT NULL,
                hit_count   INTEGER NOT NULL DEFAULT 0
            );
            CREATE INDEX IF NOT EXISTS idx_rc_accessed ON response_cache(accessed_at);
            CREATE INDEX IF NOT EXISTS idx_rc_created ON response_cache(created_at);
        )";
        
        char* err = nullptr;
        sqlite3_exec(conn_.get(), sql, nullptr, nullptr, &err);
        sqlite3_free(err);
    }
    
public:
    ResponseCache(const std::filesystem::path& workspace_dir, uint32_t ttl_minutes, size_t max_entries)
        : ttl_minutes_(ttl_minutes)
        , max_entries_(max_entries)
    {
        auto db_dir = workspace_dir / "memory";
        std::filesystem::create_directories(db_dir);
        db_path_ = db_dir / "response_cache.db";
        
        sqlite3* raw_conn = nullptr;
        int rc = sqlite3_open(db_path_.string().c_str(), &raw_conn);
        if (rc != SQLITE_OK) {
            throw MemoryError("Failed to open response cache database");
        }
        conn_.reset(raw_conn, sqlite3_close);
        
        const char* pragma = "PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL;";
        char* err = nullptr;
        sqlite3_exec(conn_.get(), pragma, nullptr, nullptr, &err);
        sqlite3_free(err);
        
        init_schema();
    }
    
    static std::string cache_key(const std::string& model,
                                 const std::optional<std::string>& system_prompt,
                                 const std::string& user_prompt)
    {
        std::string combined = model + "|" + system_prompt.value_or("") + "|" + user_prompt;
        std::hash<std::string> hasher;
        size_t h = hasher(combined);
        std::stringstream ss;
        ss << std::hex << std::setw(64) << std::setfill('0') << h;
        return ss.str();
    }
    
    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto cutoff_time = now - std::chrono::minutes(ttl_minutes_);
        auto cutoff = std::chrono::system_clock::to_time_t(cutoff_time);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&cutoff), "%Y-%m-%dT%H:%M:%S");
        std::string cutoff_str = ss.str();
        
        const char* sql = "SELECT response FROM response_cache WHERE prompt_hash = ? AND created_at > ?";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(conn_.get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, cutoff_str.c_str(), -1, SQLITE_TRANSIENT);
            
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                sqlite3_finalize(stmt);
                
                std::string now_str = current_timestamp();
                const char* update_sql = "UPDATE response_cache SET accessed_at = ?, hit_count = hit_count + 1 WHERE prompt_hash = ?";
                if (sqlite3_prepare_v2(conn_.get(), update_sql, -1, &stmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, now_str.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(stmt);
                    sqlite3_finalize(stmt);
                }
                
                return result;
            }
            sqlite3_finalize(stmt);
        }
        
        return std::nullopt;
    }
    
    void put(const std::string& key, const std::string& model,
             const std::string& response, uint32_t token_count)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string now = current_timestamp();
        
        const char* sql = "INSERT OR REPLACE INTO response_cache "
                         "(prompt_hash, model, response, token_count, created_at, accessed_at, hit_count) "
                         "VALUES (?, ?, ?, ?, ?, ?, 0)";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(conn_.get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, model.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, response.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 4, token_count);
            sqlite3_bind_text(stmt, 5, now.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 6, now.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::minutes(ttl_minutes_);
        auto cutoff = std::chrono::system_clock::to_time_t(cutoff_time);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&cutoff), "%Y-%m-%dT%H:%M:%S");
        
        const char* delete_sql = "DELETE FROM response_cache WHERE created_at <= ?";
        if (sqlite3_prepare_v2(conn_.get(), delete_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, ss.str().c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        
        const char* lru_sql = "DELETE FROM response_cache WHERE prompt_hash IN "
                             "(SELECT prompt_hash FROM response_cache ORDER BY accessed_at ASC "
                             "LIMIT MAX(0, (SELECT COUNT(*) FROM response_cache) - ?))";
        if (sqlite3_prepare_v2(conn_.get(), lru_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(max_entries_));
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    
    std::tuple<size_t, uint64_t, uint64_t> stats() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t count = 0;
        uint64_t hits = 0;
        uint64_t tokens_saved = 0;
        
        const char* sql1 = "SELECT COUNT(*) FROM response_cache";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(conn_.get(), sql1, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
            }
            sqlite3_finalize(stmt);
        }
        
        const char* sql2 = "SELECT COALESCE(SUM(hit_count), 0) FROM response_cache";
        if (sqlite3_prepare_v2(conn_.get(), sql2, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                hits = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
            }
            sqlite3_finalize(stmt);
        }
        
        const char* sql3 = "SELECT COALESCE(SUM(token_count * hit_count), 0) FROM response_cache";
        if (sqlite3_prepare_v2(conn_.get(), sql3, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                tokens_saved = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
            }
            sqlite3_finalize(stmt);
        }
        
        return {count, hits, tokens_saved};
    }
    
    size_t clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        const char* sql = "DELETE FROM response_cache";
        sqlite3_exec(conn_.get(), sql, nullptr, nullptr, nullptr);
        return static_cast<size_t>(sqlite3_changes(conn_.get()));
    }
};

}
