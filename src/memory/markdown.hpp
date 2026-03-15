#pragma once

#include "traits.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace zeroclaw::memory {

class MarkdownMemory : public Memory {
    std::filesystem::path workspace_dir_;
    
    std::filesystem::path memory_dir() const {
        return workspace_dir_ / "memory";
    }
    
    std::filesystem::path core_path() const {
        return workspace_dir_ / "MEMORY.md";
    }
    
    std::filesystem::path daily_path() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d");
        return memory_dir() / (ss.str() + ".md");
    }
    
    void ensure_dirs() {
        std::filesystem::create_directories(memory_dir());
    }
    
    void append_to_file(const std::filesystem::path& path, const std::string& content) {
        ensure_dirs();
        
        std::string existing;
        if (std::filesystem::exists(path)) {
            std::ifstream ifs(path);
            std::stringstream buf;
            buf << ifs.rdbuf();
            existing = buf.str();
        }
        
        std::string header;
        if (existing.empty()) {
            if (path == core_path()) {
                header = "# Long-Term Memory\n\n";
            } else {
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                std::stringstream ss;
                ss << std::put_time(std::localtime(&time), "%Y-%m-%d");
                header = "# Daily Log — " + ss.str() + "\n\n";
            }
        }
        
        std::ofstream ofs(path, std::ios::trunc);
        if (!existing.empty()) {
            ofs << existing << "\n" << content << "\n";
        } else {
            ofs << header << content << "\n";
        }
    }
    
    static std::vector<MemoryEntry> parse_entries_from_file(
        const std::filesystem::path& path,
        const std::string& content,
        const MemoryCategory& category)
    {
        std::string filename = path.stem().string();
        std::vector<MemoryEntry> entries;
        
        std::istringstream stream(content);
        std::string line;
        size_t i = 0;
        
        while (std::getline(stream, line)) {
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            std::string trimmed = line.substr(start);
            if (trimmed.empty() || trimmed[0] == '#') continue;
            
            std::string clean = trimmed;
            if (clean.size() > 2 && clean[0] == '-' && clean[1] == ' ') {
                clean = clean.substr(2);
            }
            
            entries.emplace_back(
                filename + ":" + std::to_string(i),
                filename + ":" + std::to_string(i),
                clean,
                category,
                filename,
                std::nullopt,
                std::nullopt
            );
            ++i;
        }
        
        return entries;
    }
    
    std::vector<MemoryEntry> read_all_entries() {
        std::vector<MemoryEntry> entries;
        
        if (std::filesystem::exists(core_path())) {
            std::ifstream ifs(core_path());
            std::stringstream buf;
            buf << ifs.rdbuf();
            auto file_entries = parse_entries_from_file(core_path(), buf.str(), MemoryCategory(MemoryCategory::Kind::Core));
            entries.insert(entries.end(), file_entries.begin(), file_entries.end());
        }
        
        if (std::filesystem::exists(memory_dir())) {
            for (const auto& entry : std::filesystem::directory_iterator(memory_dir())) {
                if (entry.path().extension() == ".md") {
                    std::ifstream ifs(entry.path());
                    std::stringstream buf;
                    buf << ifs.rdbuf();
                    auto file_entries = parse_entries_from_file(entry.path(), buf.str(), MemoryCategory(MemoryCategory::Kind::Daily));
                    entries.insert(entries.end(), file_entries.begin(), file_entries.end());
                }
            }
        }
        
        std::sort(entries.begin(), entries.end(), [](const MemoryEntry& a, const MemoryEntry& b) {
            return a.timestamp > b.timestamp;
        });
        
        return entries;
    }
    
public:
    explicit MarkdownMemory(const std::filesystem::path& workspace_dir)
        : workspace_dir_(workspace_dir) {}
    
    std::string name() const override {
        return "markdown";
    }
    
    void store(const std::string& key,
               const std::string& content,
               const MemoryCategory& category,
               const std::optional<std::string>& = std::nullopt) override
    {
        std::string entry = "- **" + key + "**: " + content;
        std::filesystem::path path;
        if (category.is_core()) {
            path = core_path();
        } else {
            path = daily_path();
        }
        append_to_file(path, entry);
    }
    
    std::vector<MemoryEntry> recall(const std::string& query,
                                    size_t limit,
                                    const std::optional<std::string>& = std::nullopt) override
    {
        auto all = read_all_entries();
        std::string query_lower = query;
        std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        
        std::vector<std::string> keywords;
        std::istringstream ks(query_lower);
        std::string word;
        while (ks >> word) {
            keywords.push_back(word);
        }
        
        std::vector<MemoryEntry> scored;
        for (auto& entry : all) {
            std::string content_lower = entry.content;
            std::transform(content_lower.begin(), content_lower.end(), content_lower.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            
            size_t matched = 0;
            for (const auto& kw : keywords) {
                if (content_lower.find(kw) != std::string::npos) {
                    ++matched;
                }
            }
            
            if (matched > 0) {
                entry.score = static_cast<double>(matched) / keywords.size();
                scored.push_back(std::move(entry));
            }
        }
        
        std::sort(scored.begin(), scored.end(), [](const MemoryEntry& a, const MemoryEntry& b) {
            return a.score > b.score;
        });
        
        if (scored.size() > limit) {
            scored.resize(limit);
        }
        
        return scored;
    }
    
    std::optional<MemoryEntry> get(const std::string& key) override {
        auto all = read_all_entries();
        for (auto& entry : all) {
            if (entry.key == key || entry.content.find(key) != std::string::npos) {
                return entry;
            }
        }
        return std::nullopt;
    }
    
    std::vector<MemoryEntry> list(const std::optional<MemoryCategory>& category = std::nullopt,
                                  const std::optional<std::string>& = std::nullopt) override
    {
        auto all = read_all_entries();
        if (category.has_value()) {
            std::vector<MemoryEntry> filtered;
            for (const auto& entry : all) {
                if (entry.category == *category) {
                    filtered.push_back(entry);
                }
            }
            return filtered;
        }
        return all;
    }
    
    bool forget(const std::string&) override {
        return false;
    }
    
    size_t count() override {
        return read_all_entries().size();
    }
    
    bool health_check() override {
        return std::filesystem::exists(workspace_dir_);
    }
};

}
