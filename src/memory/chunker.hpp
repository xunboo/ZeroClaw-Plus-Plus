#pragma once

#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <algorithm>

namespace zeroclaw::memory {

struct Chunk {
    size_t index;
    std::string content;
    std::optional<std::string> heading;
};

namespace detail {
    inline std::vector<std::pair<std::optional<std::string>, std::string>> split_on_headings(const std::string& text) {
        std::vector<std::pair<std::optional<std::string>, std::string>> sections;
        std::optional<std::string> current_heading;
        std::string current_body;
        
        std::istringstream stream(text);
        std::string line;
        
        while (std::getline(stream, line)) {
            bool is_heading = false;
            if (line.size() >= 2 && line[0] == '#') {
                size_t hash_count = 0;
                while (hash_count < line.size() && line[hash_count] == '#') hash_count++;
                if (hash_count >= 1 && hash_count <= 3 && 
                    (line.size() > hash_count && line[hash_count] == ' ')) {
                    is_heading = true;
                }
            }
            
            if (is_heading) {
                if (!current_body.empty() || current_heading.has_value()) {
                    sections.emplace_back(std::move(current_heading), std::move(current_body));
                    current_body.clear();
                }
                current_heading = line;
            } else {
                if (!current_body.empty()) current_body += '\n';
                current_body += line;
            }
        }
        
        if (!current_body.empty() || current_heading.has_value()) {
            sections.emplace_back(std::move(current_heading), std::move(current_body));
        }
        
        return sections;
    }
    
    inline std::vector<std::string> split_on_blank_lines(const std::string& text) {
        std::vector<std::string> paragraphs;
        std::string current;
        
        std::istringstream stream(text);
        std::string line;
        
        while (std::getline(stream, line)) {
            if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
                if (!current.empty()) {
                    size_t start = current.find_first_not_of(" \t\r\n");
                    if (start != std::string::npos) {
                        paragraphs.push_back(current.substr(start));
                    }
                    current.clear();
                }
            } else {
                if (!current.empty()) current += '\n';
                current += line;
            }
        }
        
        if (!current.empty()) {
            size_t start = current.find_first_not_of(" \t\r\n");
            if (start != std::string::npos) {
                paragraphs.push_back(current.substr(start));
            }
        }
        
        return paragraphs;
    }
    
    inline std::vector<std::string> split_on_lines(const std::string& text, size_t max_chars) {
        std::vector<std::string> chunks;
        std::string current;
        
        std::istringstream stream(text);
        std::string line;
        
        while (std::getline(stream, line)) {
            if (!current.empty() && current.size() + line.size() + 1 > max_chars) {
                chunks.push_back(current);
                current.clear();
            }
            if (!current.empty()) current += '\n';
            current += line;
        }
        
        if (!current.empty()) {
            chunks.push_back(current);
        }
        
        return chunks;
    }
}

inline std::vector<Chunk> chunk_markdown(const std::string& text, size_t max_tokens) {
    std::string trimmed = text;
    size_t start = trimmed.find_first_not_of(" \t\r\n");
    if (start == std::string::npos || trimmed.empty()) {
        return {};
    }
    
    size_t max_chars = max_tokens * 4;
    auto sections = detail::split_on_headings(text);
    std::vector<Chunk> chunks;
    
    for (auto& [heading, body] : sections) {
        std::string full;
        if (heading.has_value()) {
            full = *heading + "\n" + body;
        } else {
            full = body;
        }
        
        if (full.size() <= max_chars) {
            size_t content_start = full.find_first_not_of(" \t\r\n");
            if (content_start != std::string::npos) {
                chunks.push_back({chunks.size(), full.substr(content_start), heading});
            }
        } else {
            auto paragraphs = detail::split_on_blank_lines(body);
            std::string current = heading.value_or("");
            
            for (const auto& para : paragraphs) {
                if (!current.empty() && current.size() + para.size() > max_chars) {
                    size_t content_start = current.find_first_not_of(" \t\r\n");
                    if (content_start != std::string::npos) {
                        chunks.push_back({chunks.size(), current.substr(content_start), heading});
                    }
                    current = heading.value_or("");
                }
                
                if (para.size() > max_chars) {
                    if (!current.empty()) {
                        size_t content_start = current.find_first_not_of(" \t\r\n");
                        if (content_start != std::string::npos) {
                            chunks.push_back({chunks.size(), current.substr(content_start), heading});
                        }
                        current = heading.value_or("");
                    }
                    for (const auto& line_chunk : detail::split_on_lines(para, max_chars)) {
                        chunks.push_back({chunks.size(), line_chunk, heading});
                    }
                } else {
                    if (!current.empty()) current += '\n';
                    current += para;
                }
            }
            
            if (!current.empty()) {
                size_t content_start = current.find_first_not_of(" \t\r\n");
                if (content_start != std::string::npos) {
                    chunks.push_back({chunks.size(), current.substr(content_start), heading});
                }
            }
        }
    }
    
    chunks.erase(std::remove_if(chunks.begin(), chunks.end(),
        [](const Chunk& c) { return c.content.empty(); }), chunks.end());
    
    for (size_t i = 0; i < chunks.size(); ++i) {
        chunks[i].index = i;
    }
    
    return chunks;
}

}
