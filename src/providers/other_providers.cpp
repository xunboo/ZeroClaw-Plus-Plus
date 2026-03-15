#include "other_providers.hpp"
#include <thread>
#include <chrono>

namespace zeroclaw {
namespace providers {

// ── GeminiProvider ───────────────────────────────────────────────

GeminiProvider::GeminiProvider(const std::optional<std::string>& api_key)
    : api_key_(api_key.value_or("")) {}

GeminiProvider GeminiProvider::with_base_url(const std::optional<std::string>& api_key,
                                               const std::optional<std::string>& base_url) {
    GeminiProvider p(api_key);
    if (base_url.has_value()) p.base_url_ = *base_url;
    return p;
}

nlohmann::json GeminiProvider::to_gemini_contents(const std::vector<ChatMessage>& messages) {
    nlohmann::json contents = nlohmann::json::array();
    for (const auto& msg : messages) {
        if (msg.role == "system") continue;
        std::string role = (msg.role == "assistant") ? "model" : "user";
        contents.push_back({
            {"role", role},
            {"parts", nlohmann::json::array({{{"text", msg.content}}})}
        });
    }
    return contents;
}

ChatResponse GeminiProvider::chat(const ChatRequest& request,
                                    const std::string& model, double temperature) {
    (void)request; (void)model; (void)temperature;
    ChatResponse r; r.text = "[Gemini - not implemented]"; return r;
}

ChatResponse GeminiProvider::chat_with_tools(const std::vector<ChatMessage>& messages,
                                               const std::vector<ToolSpec>& tools,
                                               const std::string& model, double temperature) {
    (void)messages; (void)tools; (void)model; (void)temperature;
    ChatResponse r; r.text = "[Gemini - not implemented]"; return r;
}

void GeminiProvider::warmup() { (!api_key_.empty()); }
std::string GeminiProvider::chat_with_system(const std::optional<std::string>& sp, const std::string& msg, const std::string& model, double temp) {
    std::vector<ChatMessage> msgs;
    if (sp) msgs.push_back(ChatMessage::system(*sp));
    msgs.push_back(ChatMessage::user(msg));
    ChatRequest req; req.messages = &msgs;
    return chat(req, model, temp).text_or_empty();
}

// ── OllamaProvider ───────────────────────────────────────────────

OllamaProvider::OllamaProvider(const std::optional<std::string>& base_url) {
    if (base_url.has_value()) base_url_ = *base_url;
}

ChatResponse OllamaProvider::chat(const ChatRequest& request,
                                    const std::string& model, double temperature) {
    (void)request; (void)model; (void)temperature;
    ChatResponse r; r.text = "[Ollama - not implemented]"; return r;
}

ChatResponse OllamaProvider::chat_with_tools(const std::vector<ChatMessage>& messages,
                                               const std::vector<ToolSpec>& tools,
                                               const std::string& model, double temperature) {
    (void)messages; (void)tools; (void)model; (void)temperature;
    ChatResponse r; r.text = "[Ollama - not implemented]"; return r;
}

void OllamaProvider::warmup() { (true); }
std::string OllamaProvider::chat_with_system(const std::optional<std::string>& sp, const std::string& msg, const std::string& model, double temp) {
    std::vector<ChatMessage> msgs;
    if (sp) msgs.push_back(ChatMessage::system(*sp));
    msgs.push_back(ChatMessage::user(msg));
    ChatRequest req; req.messages = &msgs;
    return chat(req, model, temp).text_or_empty();
}

// ── BedrockProvider ──────────────────────────────────────────────

BedrockProvider::BedrockProvider(const std::string& region,
                                  const std::optional<std::string>& profile)
    : region_(region), profile_(profile) {}

ChatResponse BedrockProvider::chat(const ChatRequest& request,
                                     const std::string& model, double temperature) {
    (void)request; (void)model; (void)temperature;
    ChatResponse r; r.text = "[Bedrock - not implemented]"; return r;
}

ChatResponse BedrockProvider::chat_with_tools(const std::vector<ChatMessage>& messages,
                                                const std::vector<ToolSpec>& tools,
                                                const std::string& model, double temperature) {
    (void)messages; (void)tools; (void)model; (void)temperature;
    ChatResponse r; r.text = "[Bedrock - not implemented]"; return r;
}

void BedrockProvider::warmup() { (true); }
std::string BedrockProvider::chat_with_system(const std::optional<std::string>& sp, const std::string& msg, const std::string& model, double temp) {
    std::vector<ChatMessage> msgs;
    if (sp) msgs.push_back(ChatMessage::system(*sp));
    msgs.push_back(ChatMessage::user(msg));
    ChatRequest req; req.messages = &msgs;
    return chat(req, model, temp).text_or_empty();
}

// ── OpenRouterProvider ───────────────────────────────────────────

OpenRouterProvider::OpenRouterProvider(const std::optional<std::string>& api_key)
    : api_key_(api_key.value_or("")) {}

ChatResponse OpenRouterProvider::chat(const ChatRequest& request,
                                        const std::string& model, double temperature) {
    (void)request; (void)model; (void)temperature;
    ChatResponse r; r.text = "[OpenRouter - not implemented]"; return r;
}

ChatResponse OpenRouterProvider::chat_with_tools(const std::vector<ChatMessage>& messages,
                                                   const std::vector<ToolSpec>& tools,
                                                   const std::string& model, double temperature) {
    (void)messages; (void)tools; (void)model; (void)temperature;
    ChatResponse r; r.text = "[OpenRouter - not implemented]"; return r;
}

void OpenRouterProvider::warmup() { (!api_key_.empty()); }
std::string OpenRouterProvider::chat_with_system(const std::optional<std::string>& sp, const std::string& msg, const std::string& model, double temp) {
    std::vector<ChatMessage> msgs;
    if (sp) msgs.push_back(ChatMessage::system(*sp));
    msgs.push_back(ChatMessage::user(msg));
    ChatRequest req; req.messages = &msgs;
    return chat(req, model, temp).text_or_empty();
}

// ── CompatibleProvider ───────────────────────────────────────────

CompatibleProvider::CompatibleProvider(const std::string& name,
                                        const std::string& base_url,
                                        const std::optional<std::string>& api_key,
                                        AuthStyle auth_style)
    : name_(name), base_url_(base_url), api_key_(api_key.value_or("")),
      auth_style_(auth_style) {}

ChatResponse CompatibleProvider::chat(const ChatRequest& request,
                                        const std::string& model, double temperature) {
    (void)request; (void)model; (void)temperature;
    ChatResponse r; r.text = "[Compatible - not implemented]"; return r;
}

ChatResponse CompatibleProvider::chat_with_tools(const std::vector<ChatMessage>& messages,
                                                   const std::vector<ToolSpec>& tools,
                                                   const std::string& model, double temperature) {
    (void)messages; (void)tools; (void)model; (void)temperature;
    ChatResponse r; r.text = "[Compatible - not implemented]"; return r;
}

void CompatibleProvider::warmup() { (true); }
std::string CompatibleProvider::chat_with_system(const std::optional<std::string>& sp, const std::string& msg, const std::string& model, double temp) {
    std::vector<ChatMessage> msgs;
    if (sp) msgs.push_back(ChatMessage::system(*sp));
    msgs.push_back(ChatMessage::user(msg));
    ChatRequest req; req.messages = &msgs;
    return chat(req, model, temp).text_or_empty();
}

// ── ReliableProvider ─────────────────────────────────────────────

ReliableProvider::ReliableProvider(std::unique_ptr<Provider> inner,
                                    int max_retries, int retry_delay_ms)
    : inner_(std::move(inner)), max_retries_(max_retries),
      retry_delay_ms_(retry_delay_ms) {}

std::string ReliableProvider::name() const {
    return "reliable(" + (inner_ ? inner_->name() : "null") + ")";
}

ChatResponse ReliableProvider::chat(const ChatRequest& request,
                                      const std::string& model, double temperature) {
    for (int attempt = 0; attempt <= max_retries_; ++attempt) {
        try {
            return inner_->chat(request, model, temperature);
        } catch (...) {
            if (attempt == max_retries_) throw;
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms_));
        }
    }
    ChatResponse r;
    r.text = "[Reliable - all retries exhausted]";
    return r;
}

ChatResponse ReliableProvider::chat_with_tools(const std::vector<ChatMessage>& messages,
                                                 const std::vector<ToolSpec>& tools,
                                                 const std::string& model, double temperature) {
    for (int attempt = 0; attempt <= max_retries_; ++attempt) {
        try {
            return inner_->chat_with_tools(messages, tools, model, temperature);
        } catch (...) {
            if (attempt == max_retries_) throw;
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms_));
        }
    }
    ChatResponse r;
    r.text = "[Reliable - all retries exhausted]";
    return r;
}

void ReliableProvider::warmup() { (inner_ ? inner_->warmup() : false); }
std::string ReliableProvider::chat_with_system(const std::optional<std::string>& sp, const std::string& msg, const std::string& model, double temp) {
    std::vector<ChatMessage> msgs;
    if (sp) msgs.push_back(ChatMessage::system(*sp));
    msgs.push_back(ChatMessage::user(msg));
    ChatRequest req; req.messages = &msgs;
    return chat(req, model, temp).text_or_empty();
}

// ── RouterProvider ───────────────────────────────────────────────

void RouterProvider::add_provider(const std::string& prefix,
                                    std::unique_ptr<Provider> provider) {
    providers_.emplace_back(prefix, std::move(provider));
}

Provider* RouterProvider::resolve_provider(const std::string& model) {
    for (const auto& [prefix, provider] : providers_) {
        if (model.find(prefix) == 0 || model.find(prefix + "/") != std::string::npos) {
            return provider.get();
        }
    }
    return providers_.empty() ? nullptr : providers_[0].second.get();
}

ChatResponse RouterProvider::chat(const ChatRequest& request,
                                    const std::string& model, double temperature) {
    auto* p = resolve_provider(model);
    if (!p) { ChatResponse r; r.text = "[Router - no provider]"; return r; }
    return p->chat(request, model, temperature);
}

ChatResponse RouterProvider::chat_with_tools(const std::vector<ChatMessage>& messages,
                                               const std::vector<ToolSpec>& tools,
                                               const std::string& model, double temperature) {
    auto* p = resolve_provider(model);
    if (!p) { ChatResponse r; r.text = "[Router - no provider]"; return r; }
    return p->chat_with_tools(messages, tools, model, temperature);
}

void RouterProvider::warmup() {
    for (auto& [_, p] : providers_) p->warmup();
}

std::string RouterProvider::chat_with_system(const std::optional<std::string>& sp, const std::string& msg, const std::string& model, double temp) {
    std::vector<ChatMessage> msgs;
    if (sp) msgs.push_back(ChatMessage::system(*sp));
    msgs.push_back(ChatMessage::user(msg));
    ChatRequest req; req.messages = &msgs;
    return chat(req, model, temp).text_or_empty();
}


// ── Remaining stubs ──────────────────────────────────────────────

GlmProvider::GlmProvider(const std::optional<std::string>& api_key, const std::string& base_url)
    : api_key_(api_key.value_or("")), base_url_(base_url) {}
ChatResponse GlmProvider::chat(const ChatRequest&, const std::string&, double) { return {}; }
ChatResponse GlmProvider::chat_with_tools(const std::vector<ChatMessage>&, const std::vector<ToolSpec>&, const std::string&, double) { return {}; }
void GlmProvider::warmup() { (!api_key_.empty()); }
std::string GlmProvider::chat_with_system(const std::optional<std::string>& sp, const std::string& msg, const std::string& model, double temp) {
    std::vector<ChatMessage> msgs;
    if (sp) msgs.push_back(ChatMessage::system(*sp));
    msgs.push_back(ChatMessage::user(msg));
    ChatRequest req; req.messages = &msgs;
    return chat(req, model, temp).text_or_empty();
}

TelnyxProvider::TelnyxProvider(const std::optional<std::string>& api_key)
    : api_key_(api_key.value_or("")) {}
ChatResponse TelnyxProvider::chat(const ChatRequest&, const std::string&, double) { return {}; }
ChatResponse TelnyxProvider::chat_with_tools(const std::vector<ChatMessage>&, const std::vector<ToolSpec>&, const std::string&, double) { return {}; }
void TelnyxProvider::warmup() { (!api_key_.empty()); }
std::string TelnyxProvider::chat_with_system(const std::optional<std::string>& sp, const std::string& msg, const std::string& model, double temp) {
    std::vector<ChatMessage> msgs;
    if (sp) msgs.push_back(ChatMessage::system(*sp));
    msgs.push_back(ChatMessage::user(msg));
    ChatRequest req; req.messages = &msgs;
    return chat(req, model, temp).text_or_empty();
}

CopilotProvider::CopilotProvider(const std::optional<std::string>& token)
    : token_(token.value_or("")) {}
ChatResponse CopilotProvider::chat(const ChatRequest&, const std::string&, double) { return {}; }
ChatResponse CopilotProvider::chat_with_tools(const std::vector<ChatMessage>&, const std::vector<ToolSpec>&, const std::string&, double) { return {}; }
void CopilotProvider::warmup() { (!token_.empty()); }
std::string CopilotProvider::chat_with_system(const std::optional<std::string>& sp, const std::string& msg, const std::string& model, double temp) {
    std::vector<ChatMessage> msgs;
    if (sp) msgs.push_back(ChatMessage::system(*sp));
    msgs.push_back(ChatMessage::user(msg));
    ChatRequest req; req.messages = &msgs;
    return chat(req, model, temp).text_or_empty();
}

OpenAiCodexProvider::OpenAiCodexProvider(const std::optional<std::string>& api_key)
    : api_key_(api_key.value_or("")) {}
ChatResponse OpenAiCodexProvider::chat(const ChatRequest&, const std::string&, double) { return {}; }
ChatResponse OpenAiCodexProvider::chat_with_tools(const std::vector<ChatMessage>&, const std::vector<ToolSpec>&, const std::string&, double) { return {}; }
void OpenAiCodexProvider::warmup() { (!api_key_.empty()); }
std::string OpenAiCodexProvider::chat_with_system(const std::optional<std::string>& sp, const std::string& msg, const std::string& model, double temp) {
    std::vector<ChatMessage> msgs;
    if (sp) msgs.push_back(ChatMessage::system(*sp));
    msgs.push_back(ChatMessage::user(msg));
    ChatRequest req; req.messages = &msgs;
    return chat(req, model, temp).text_or_empty();
}

} // namespace providers
} // namespace zeroclaw
