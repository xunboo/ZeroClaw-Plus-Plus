#pragma once

/// Remaining provider implementations — Gemini, Ollama, Bedrock, OpenRouter,
/// Compatible (OpenAI-compatible), Reliable (retry wrapper), Router, GLM, Telnyx, Copilot.

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "traits.hpp"
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace providers {

// ── Gemini ───────────────────────────────────────────────────────

/// Google Gemini provider — uses the generativelanguage.googleapis.com API
class GeminiProvider : public Provider {
public:
    explicit GeminiProvider(const std::optional<std::string>& api_key = std::nullopt);

    static GeminiProvider with_base_url(const std::optional<std::string>& api_key,
                                         const std::optional<std::string>& base_url);

    std::string name() const override { return "gemini"; }
    ChatResponse chat(const ChatRequest& request,
                       const std::string& model, double temperature) override;
    ChatResponse chat_with_tools(const std::vector<ChatMessage>& messages,
                                   const std::vector<ToolSpec>& tools,
                                   const std::string& model, double temperature) override;
    std::string chat_with_system(const std::optional<std::string>& system_prompt,
                                 const std::string& message,
                                 const std::string& model, double temperature) override;
    void warmup() override;

private:
    static nlohmann::json to_gemini_contents(const std::vector<ChatMessage>& messages);
    std::string api_key_;
    std::string base_url_ = "https://generativelanguage.googleapis.com/v1beta";
};

// ── Ollama ───────────────────────────────────────────────────────

/// Ollama provider — local LLM inference via Ollama's REST API
class OllamaProvider : public Provider {
public:
    explicit OllamaProvider(const std::optional<std::string>& base_url = std::nullopt);

    std::string name() const override { return "ollama"; }
    ChatResponse chat(const ChatRequest& request,
                       const std::string& model, double temperature) override;
    ChatResponse chat_with_tools(const std::vector<ChatMessage>& messages,
                                   const std::vector<ToolSpec>& tools,
                                   const std::string& model, double temperature) override;
    std::string chat_with_system(const std::optional<std::string>& system_prompt,
                                 const std::string& message,
                                 const std::string& model, double temperature) override;
    void warmup() override;

private:
    std::string base_url_ = "http://localhost:11434";
};

// ── Bedrock ──────────────────────────────────────────────────────

/// AWS Bedrock provider — uses AWS SDK for model inference
class BedrockProvider : public Provider {
public:
    BedrockProvider(const std::string& region = "us-east-1",
                    const std::optional<std::string>& profile = std::nullopt);

    std::string name() const override { return "bedrock"; }
    ChatResponse chat(const ChatRequest& request,
                       const std::string& model, double temperature) override;
    ChatResponse chat_with_tools(const std::vector<ChatMessage>& messages,
                                   const std::vector<ToolSpec>& tools,
                                   const std::string& model, double temperature) override;
    std::string chat_with_system(const std::optional<std::string>& system_prompt,
                                 const std::string& message,
                                 const std::string& model, double temperature) override;
    void warmup() override;

private:
    std::string region_;
    std::optional<std::string> profile_;
};

// ── OpenRouter ───────────────────────────────────────────────────

/// OpenRouter provider — unified API for multiple model providers
class OpenRouterProvider : public Provider {
public:
    explicit OpenRouterProvider(const std::optional<std::string>& api_key = std::nullopt);

    std::string name() const override { return "openrouter"; }
    ChatResponse chat(const ChatRequest& request,
                       const std::string& model, double temperature) override;
    ChatResponse chat_with_tools(const std::vector<ChatMessage>& messages,
                                   const std::vector<ToolSpec>& tools,
                                   const std::string& model, double temperature) override;
    std::string chat_with_system(const std::optional<std::string>& system_prompt,
                                 const std::string& message,
                                 const std::string& model, double temperature) override;
    void warmup() override;

private:
    std::string api_key_;
    std::string base_url_ = "https://openrouter.ai/api/v1";
};

// ── OpenAI-Compatible ────────────────────────────────────────────

/// Authentication style for compatible providers
enum class AuthStyle {
    Bearer,   // Authorization: Bearer <key>
    ApiKey,   // x-api-key: <key>
    None      // No auth header
};

/// Generic OpenAI-compatible provider (used by many self-hosted and vendor APIs)
class CompatibleProvider : public Provider {
public:
    CompatibleProvider(const std::string& name,
                       const std::string& base_url,
                       const std::optional<std::string>& api_key = std::nullopt,
                       AuthStyle auth_style = AuthStyle::Bearer);

    std::string name() const override { return name_; }
    ChatResponse chat(const ChatRequest& request,
                       const std::string& model, double temperature) override;
    ChatResponse chat_with_tools(const std::vector<ChatMessage>& messages,
                                   const std::vector<ToolSpec>& tools,
                                   const std::string& model, double temperature) override;
    std::string chat_with_system(const std::optional<std::string>& system_prompt,
                                 const std::string& message,
                                 const std::string& model, double temperature) override;
    void warmup() override;

private:
    std::string name_;
    std::string base_url_;
    std::string api_key_;
    AuthStyle auth_style_;
};

// ── Reliable (retry wrapper) ─────────────────────────────────────

/// Retry/fallback wrapper around another provider
class ReliableProvider : public Provider {
public:
    ReliableProvider(std::unique_ptr<Provider> inner,
                     int max_retries = 3,
                     int retry_delay_ms = 1000);

    std::string name() const override;
    ChatResponse chat(const ChatRequest& request,
                       const std::string& model, double temperature) override;
    ChatResponse chat_with_tools(const std::vector<ChatMessage>& messages,
                                   const std::vector<ToolSpec>& tools,
                                   const std::string& model, double temperature) override;
    std::string chat_with_system(const std::optional<std::string>& system_prompt,
                                 const std::string& message,
                                 const std::string& model, double temperature) override;
    void warmup() override;

private:
    std::unique_ptr<Provider> inner_;
    int max_retries_;
    int retry_delay_ms_;
};

// ── Router ───────────────────────────────────────────────────────

/// Routes requests to different providers based on model prefix
class RouterProvider : public Provider {
public:
    void add_provider(const std::string& prefix, std::unique_ptr<Provider> provider);

    std::string name() const override { return "router"; }
    ChatResponse chat(const ChatRequest& request,
                       const std::string& model, double temperature) override;
    ChatResponse chat_with_tools(const std::vector<ChatMessage>& messages,
                                   const std::vector<ToolSpec>& tools,
                                   const std::string& model, double temperature) override;
    std::string chat_with_system(const std::optional<std::string>& system_prompt,
                                 const std::string& message,
                                 const std::string& model, double temperature) override;
    void warmup() override;

private:
    Provider* resolve_provider(const std::string& model);
    std::vector<std::pair<std::string, std::unique_ptr<Provider>>> providers_;
};

// ── GLM ──────────────────────────────────────────────────────────

/// GLM (ChatGLM/Z.AI) provider
class GlmProvider : public Provider {
public:
    GlmProvider(const std::optional<std::string>& api_key,
                const std::string& base_url = "https://open.bigmodel.cn/api/coding/paas/v4");

    std::string name() const override { return "glm"; }
    ChatResponse chat(const ChatRequest& request,
                       const std::string& model, double temperature) override;
    ChatResponse chat_with_tools(const std::vector<ChatMessage>& messages,
                                   const std::vector<ToolSpec>& tools,
                                   const std::string& model, double temperature) override;
    std::string chat_with_system(const std::optional<std::string>& system_prompt,
                                 const std::string& message,
                                 const std::string& model, double temperature) override;
    void warmup() override;

private:
    std::string api_key_;
    std::string base_url_;
};

// ── Telnyx ───────────────────────────────────────────────────────

/// Telnyx provider — voice/telecom AI models
class TelnyxProvider : public Provider {
public:
    explicit TelnyxProvider(const std::optional<std::string>& api_key = std::nullopt);

    std::string name() const override { return "telnyx"; }
    ChatResponse chat(const ChatRequest& request,
                       const std::string& model, double temperature) override;
    ChatResponse chat_with_tools(const std::vector<ChatMessage>& messages,
                                   const std::vector<ToolSpec>& tools,
                                   const std::string& model, double temperature) override;
    std::string chat_with_system(const std::optional<std::string>& system_prompt,
                                 const std::string& message,
                                 const std::string& model, double temperature) override;
    void warmup() override;

private:
    std::string api_key_;
    std::string base_url_ = "https://api.telnyx.com/v2";
};

// ── Copilot ──────────────────────────────────────────────────────

/// GitHub Copilot provider — uses Copilot plugin API
class CopilotProvider : public Provider {
public:
    explicit CopilotProvider(const std::optional<std::string>& token = std::nullopt);

    std::string name() const override { return "copilot"; }
    ChatResponse chat(const ChatRequest& request,
                       const std::string& model, double temperature) override;
    ChatResponse chat_with_tools(const std::vector<ChatMessage>& messages,
                                   const std::vector<ToolSpec>& tools,
                                   const std::string& model, double temperature) override;
    std::string chat_with_system(const std::optional<std::string>& system_prompt,
                                 const std::string& message,
                                 const std::string& model, double temperature) override;
    void warmup() override;

private:
    std::string token_;
};

// ── OpenAI Codex ─────────────────────────────────────────────────

/// OpenAI Codex provider — specialized code completion models
class OpenAiCodexProvider : public Provider {
public:
    explicit OpenAiCodexProvider(const std::optional<std::string>& api_key = std::nullopt);

    std::string name() const override { return "openai_codex"; }
    ChatResponse chat(const ChatRequest& request,
                       const std::string& model, double temperature) override;
    ChatResponse chat_with_tools(const std::vector<ChatMessage>& messages,
                                   const std::vector<ToolSpec>& tools,
                                   const std::string& model, double temperature) override;
    std::string chat_with_system(const std::optional<std::string>& system_prompt,
                                 const std::string& message,
                                 const std::string& model, double temperature) override;
    void warmup() override;

private:
    std::string api_key_;
    std::string base_url_ = "https://api.openai.com/v1";
};

} // namespace providers
} // namespace zeroclaw
