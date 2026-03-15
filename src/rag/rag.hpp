#pragma once

/// RAG (Retrieval-Augmented Generation) module — document indexing and retrieval.

#include <string>
#include <vector>
#include <optional>
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace rag {

/// A document chunk with embedding metadata
struct DocumentChunk {
    std::string id;
    std::string content;
    std::string source;          ///< file path or URL
    size_t chunk_index = 0;
    std::vector<float> embedding;
    nlohmann::json metadata;
};

/// RAG query result
struct RagResult {
    std::string content;
    std::string source;
    double relevance_score = 0.0;
};

/// RAG index — manages document chunks and retrieval
class RagIndex {
public:
    /// Index a document from a file path
    bool index_file(const std::string& file_path, size_t chunk_size = 1024);

    /// Index raw text content
    bool index_text(const std::string& content, const std::string& source,
                     size_t chunk_size = 1024);

    /// Query the index for relevant chunks
    std::vector<RagResult> query(const std::string& query, size_t top_k = 5) const;

    /// Number of indexed chunks
    size_t chunk_count() const { return chunks_.size(); }

    /// Clear all indexed data
    void clear() { chunks_.clear(); }

private:
    std::vector<DocumentChunk> chunks_;
};

} // namespace rag
} // namespace zeroclaw
