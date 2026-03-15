#include "rag.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace zeroclaw {
namespace rag {

bool RagIndex::index_file(const std::string& file_path, size_t chunk_size) {
    std::ifstream file(file_path);
    if (!file.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return index_text(content, file_path, chunk_size);
}

bool RagIndex::index_text(const std::string& content, const std::string& source,
                            size_t chunk_size) {
    size_t pos = 0;
    size_t idx = 0;
    while (pos < content.size()) {
        size_t end = std::min(pos + chunk_size, content.size());
        // Try to break at a word boundary
        if (end < content.size()) {
            auto space = content.rfind(' ', end);
            if (space > pos) end = space;
        }
        DocumentChunk chunk;
        chunk.id = source + "_" + std::to_string(idx);
        chunk.content = content.substr(pos, end - pos);
        chunk.source = source;
        chunk.chunk_index = idx;
        chunks_.push_back(chunk);
        pos = end;
        ++idx;
    }
    return true;
}

std::vector<RagResult> RagIndex::query(const std::string& query, size_t top_k) const {
    // Simple keyword matching — full implementation would use embeddings
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

    std::vector<std::pair<double, size_t>> scored;
    for (size_t i = 0; i < chunks_.size(); ++i) {
        std::string lower_content = chunks_[i].content;
        std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), ::tolower);
        size_t count = 0;
        size_t pos = 0;
        while ((pos = lower_content.find(lower_query, pos)) != std::string::npos) {
            ++count;
            pos += lower_query.size();
        }
        if (count > 0) {
            scored.emplace_back(static_cast<double>(count), i);
        }
    }

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<RagResult> results;
    for (size_t i = 0; i < std::min(top_k, scored.size()); ++i) {
        const auto& chunk = chunks_[scored[i].second];
        results.push_back({chunk.content, chunk.source, scored[i].first});
    }
    return results;
}

} // namespace rag
} // namespace zeroclaw
