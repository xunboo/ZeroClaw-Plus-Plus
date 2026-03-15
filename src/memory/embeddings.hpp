#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace zeroclaw::memory {

class EmbeddingProvider {
public:
    virtual ~EmbeddingProvider() = default;
    
    virtual std::string name() const = 0;
    virtual size_t dimensions() const = 0;
    virtual std::vector<std::vector<float>> embed(const std::vector<std::string>& texts) = 0;
    
    std::vector<float> embed_one(const std::string& text) {
        auto results = embed({text});
        if (results.empty()) {
            throw MemoryError("Empty embedding result");
        }
        return std::move(results[0]);
    }
};

class NoopEmbedding : public EmbeddingProvider {
public:
    std::string name() const override { return "none"; }
    size_t dimensions() const override { return 0; }
    
    std::vector<std::vector<float>> embed(const std::vector<std::string>&) override {
        return {};
    }
};

class OpenAiEmbedding : public EmbeddingProvider {
    std::string base_url_;
    std::string api_key_;
    std::string model_;
    size_t dims_;
    
public:
    OpenAiEmbedding(std::string base_url, std::string api_key, std::string model, size_t dims)
        : base_url_(std::move(base_url)), api_key_(std::move(api_key)), 
          model_(std::move(model)), dims_(dims) {
        while (!base_url_.empty() && base_url_.back() == '/') {
            base_url_.pop_back();
        }
    }
    
    std::string name() const override { return "openai"; }
    size_t dimensions() const override { return dims_; }
    
    std::vector<std::vector<float>> embed(const std::vector<std::string>& texts) override {
        if (texts.empty()) {
            return {};
        }
        throw MemoryError("HTTP client integration required for embedding API calls");
    }
    
    const std::string& base_url() const { return base_url_; }
    const std::string& model() const { return model_; }
};

inline std::unique_ptr<EmbeddingProvider> create_embedding_provider(
    const std::string& provider,
    const std::optional<std::string>& api_key,
    const std::string& model,
    size_t dims)
{
    std::string key = api_key.value_or("");
    
    if (provider == "openai") {
        return std::make_unique<OpenAiEmbedding>(
            "https://api.openai.com", key, model, dims);
    }
    if (provider == "openrouter") {
        return std::make_unique<OpenAiEmbedding>(
            "https://openrouter.ai/api/v1", key, model, dims);
    }
    if (provider.size() > 7 && provider.substr(0, 7) == "custom:") {
        std::string base_url = provider.substr(7);
        return std::make_unique<OpenAiEmbedding>(base_url, key, model, dims);
    }
    
    return std::make_unique<NoopEmbedding>();
}

}
