#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <optional>
#include <cstring>

namespace zeroclaw::memory {

inline float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) {
        return 0.0f;
    }
    
    double dot = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;
    
    for (size_t i = 0; i < a.size(); ++i) {
        double x = static_cast<double>(a[i]);
        double y = static_cast<double>(b[i]);
        dot += x * y;
        norm_a += x * x;
        norm_b += y * y;
    }
    
    double denom = std::sqrt(norm_a) * std::sqrt(norm_b);
    if (!std::isfinite(denom) || denom < std::numeric_limits<double>::epsilon()) {
        return 0.0f;
    }
    
    double raw = dot / denom;
    if (!std::isfinite(raw)) {
        return 0.0f;
    }
    
    return static_cast<float>(std::max(0.0, std::min(1.0, raw)));
}

inline std::vector<uint8_t> vec_to_bytes(const std::vector<float>& v) {
    std::vector<uint8_t> bytes;
    bytes.reserve(v.size() * 4);
    for (float f : v) {
        uint32_t bits;
        std::memcpy(&bits, &f, 4);
        bytes.push_back(static_cast<uint8_t>(bits & 0xFF));
        bytes.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
    }
    return bytes;
}

inline std::vector<float> bytes_to_vec(const std::vector<uint8_t>& bytes) {
    std::vector<float> result;
    result.reserve(bytes.size() / 4);
    
    for (size_t i = 0; i + 3 < bytes.size(); i += 4) {
        uint32_t bits = static_cast<uint32_t>(bytes[i]) |
                       (static_cast<uint32_t>(bytes[i + 1]) << 8) |
                       (static_cast<uint32_t>(bytes[i + 2]) << 16) |
                       (static_cast<uint32_t>(bytes[i + 3]) << 24);
        float f;
        std::memcpy(&f, &bits, 4);
        result.push_back(f);
    }
    return result;
}

struct ScoredResult {
    std::string id;
    std::optional<float> vector_score;
    std::optional<float> keyword_score;
    float final_score;
};

inline std::vector<ScoredResult> hybrid_merge(
    const std::vector<std::pair<std::string, float>>& vector_results,
    const std::vector<std::pair<std::string, float>>& keyword_results,
    float vector_weight,
    float keyword_weight,
    size_t limit)
{
    std::unordered_map<std::string, ScoredResult> map;
    
    for (const auto& [id, score] : vector_results) {
        ScoredResult r;
        r.id = id;
        r.vector_score = score;
        r.final_score = 0.0f;
        map[id] = r;
    }
    
    float max_kw = 0.0f;
    for (const auto& [_, score] : keyword_results) {
        max_kw = std::max(max_kw, score);
    }
    if (max_kw < std::numeric_limits<float>::epsilon()) {
        max_kw = 1.0f;
    }
    
    for (const auto& [id, score] : keyword_results) {
        float normalized = score / max_kw;
        auto it = map.find(id);
        if (it != map.end()) {
            it->second.keyword_score = normalized;
        } else {
            ScoredResult r;
            r.id = id;
            r.keyword_score = normalized;
            r.final_score = 0.0f;
            map[id] = r;
        }
    }
    
    std::vector<ScoredResult> results;
    results.reserve(map.size());
    for (auto& [_, r] : map) {
        float vs = r.vector_score.value_or(0.0f);
        float ks = r.keyword_score.value_or(0.0f);
        r.final_score = vector_weight * vs + keyword_weight * ks;
        results.push_back(r);
    }
    
    std::sort(results.begin(), results.end(),
              [](const ScoredResult& a, const ScoredResult& b) {
                  return a.final_score > b.final_score;
              });
    
    if (results.size() > limit) {
        results.resize(limit);
    }
    
    return results;
}

}
