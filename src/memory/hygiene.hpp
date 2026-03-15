#pragma once

#include "traits.hpp"
#include "sqlite.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <unordered_map>

namespace zeroclaw::memory {

struct HygieneConfig {
    bool enabled = true;
    uint32_t archive_after_days = 7;
    uint32_t purge_after_days = 30;
    uint32_t conversation_retention_days = 30;
};

namespace detail {
    inline std::optional<std::string> parse_date_prefix(const std::string& filename) {
        if (filename.size() < 10) return std::nullopt;
        return filename.substr(0, 10);
    }
    
    inline std::filesystem::path unique_archive_target(const std::filesystem::path& archive_dir, const std::string& filename) {
        auto direct = archive_dir / filename;
        if (!std::filesystem::exists(direct)) {
            return direct;
        }
        
        std::string stem = std::filesystem::path(filename).stem().string();
        std::string ext = std::filesystem::path(filename).extension().string();
        
        for (int i = 1; i < 10000; ++i) {
            std::string new_name = stem + "_" + std::to_string(i) + ext;
            auto candidate = archive_dir / new_name;
            if (!std::filesystem::exists(candidate)) {
                return candidate;
            }
        }
        
        return direct;
    }
    
    inline void move_to_archive(const std::filesystem::path& src, const std::filesystem::path& archive_dir) {
        std::string filename = src.filename().string();
        auto target = unique_archive_target(archive_dir, filename);
        std::filesystem::rename(src, target);
    }
    
    inline std::chrono::system_clock::time_point parse_date(const std::string& date_str) {
        std::tm tm = {};
        std::istringstream ss(date_str);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
    
    inline std::string format_date(std::chrono::system_clock::time_point tp) {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d");
        return ss.str();
    }
}

inline void run_hygiene(const HygieneConfig& config, const std::filesystem::path& workspace_dir) {
    if (!config.enabled) return;
    
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::hours(24 * config.archive_after_days);
    std::string cutoff_str = detail::format_date(cutoff);
    
    auto memory_dir = workspace_dir / "memory";
    if (std::filesystem::exists(memory_dir)) {
        auto archive_dir = memory_dir / "archive";
        std::filesystem::create_directories(archive_dir);
        
        for (const auto& entry : std::filesystem::directory_iterator(memory_dir)) {
            if (entry.is_directory()) continue;
            if (entry.path().extension() != ".md") continue;
            
            std::string filename = entry.path().stem().string();
            auto date_prefix = detail::parse_date_prefix(filename);
            if (date_prefix && *date_prefix < cutoff_str) {
                detail::move_to_archive(entry.path(), archive_dir);
            }
        }
    }
    
    auto sessions_dir = workspace_dir / "sessions";
    if (std::filesystem::exists(sessions_dir)) {
        auto archive_dir = sessions_dir / "archive";
        std::filesystem::create_directories(archive_dir);
        
        for (const auto& entry : std::filesystem::directory_iterator(sessions_dir)) {
            if (entry.is_directory()) continue;
            
            std::string filename = entry.path().filename().string();
            auto date_prefix = detail::parse_date_prefix(filename);
            if (date_prefix && *date_prefix < cutoff_str) {
                detail::move_to_archive(entry.path(), archive_dir);
            }
        }
    }
    
    if (config.purge_after_days > 0) {
        auto purge_cutoff = now - std::chrono::hours(24 * config.purge_after_days);
        std::string purge_cutoff_str = detail::format_date(purge_cutoff);
        
        auto memory_archive = memory_dir / "archive";
        if (std::filesystem::exists(memory_archive)) {
            for (const auto& entry : std::filesystem::directory_iterator(memory_archive)) {
                if (entry.is_directory()) continue;
                std::string filename = entry.path().stem().string();
                auto date_prefix = detail::parse_date_prefix(filename);
                if (date_prefix && *date_prefix < purge_cutoff_str) {
                    std::filesystem::remove(entry.path());
                }
            }
        }
        
        auto sessions_archive = sessions_dir / "archive";
        if (std::filesystem::exists(sessions_archive)) {
            for (const auto& entry : std::filesystem::directory_iterator(sessions_archive)) {
                if (entry.is_directory()) continue;
                std::string filename = entry.path().filename().string();
                auto date_prefix = detail::parse_date_prefix(filename);
                if (date_prefix && *date_prefix < purge_cutoff_str) {
                    std::filesystem::remove(entry.path());
                }
            }
        }
    }
}

}
