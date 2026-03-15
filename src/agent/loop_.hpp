#pragma once

/// Agentic loop — the main processing loop for handling messages through channels.
///
/// Contains helper functions for credential scrubbing, history management,
/// context building, tool call parsing, and the main `process_message` / `run` functions.
///
/// Note: The Rust version of this file (loop_.rs) is 5374 lines with heavy use of
/// async/await, Tokio channels, cancellation tokens, and streaming. This C++17 conversion
/// provides the core synchronous logic with the same data structures and algorithms.

#include <string>
#include <vector>
#include <optional>
#include <regex>
#include <algorithm>
#include <sstream>
#include <functional>
#include <set>
#include <chrono>
#include <memory>

#include "../providers/traits.hpp"
#include "../tools/traits.hpp"
#include "dispatcher.hpp"
#include "memory_loader.hpp"

namespace zeroclaw {
namespace agent {

using providers::ChatMessage;
using providers::ChatResponse;
using providers::Provider;

// ── Constants ────────────────────────────────────────────────────

/// Minimum characters per chunk when relaying LLM text to a streaming draft.
static constexpr size_t STREAM_CHUNK_MIN_CHARS = 80;

/// Default maximum agentic tool-use iterations per user message.
static constexpr size_t DEFAULT_MAX_TOOL_ITERATIONS = 10;

/// Minimum user-message length for auto-save to memory.
static constexpr size_t AUTOSAVE_MIN_MESSAGE_CHARS = 20;

/// Default trigger for auto-compaction when non-system message count exceeds this.
static constexpr size_t DEFAULT_MAX_HISTORY_MESSAGES = 50;

/// Keep this many most-recent non-system messages after compaction.
static constexpr size_t COMPACTION_KEEP_RECENT_MESSAGES = 20;

/// Safety cap for compaction source transcript.
static constexpr size_t COMPACTION_MAX_SOURCE_CHARS = 12000;

/// Max characters retained in stored compaction summary.
static constexpr size_t COMPACTION_MAX_SUMMARY_CHARS = 2000;

/// Minimum interval between progress sends to avoid flooding.
static constexpr uint64_t PROGRESS_MIN_INTERVAL_MS = 500;

/// Sentinel value to clear accumulated draft text.
static constexpr const char* DRAFT_CLEAR_SENTINEL = "\x00" "CLEAR" "\x00";

// ── Credential scrubbing ─────────────────────────────────────────

/// Scrub credentials from tool output to prevent accidental exfiltration.
/// Replaces known credential patterns with a redacted placeholder.
std::string scrub_credentials(const std::string& input);

// ── History management ───────────────────────────────────────────

/// Trim conversation history to prevent unbounded growth.
/// Preserves the system prompt and the most recent messages.
void trim_history(std::vector<ChatMessage>& history, size_t max_history);

/// Build compaction transcript from older messages.
std::string build_compaction_transcript(const std::vector<ChatMessage>& messages);

/// Apply a compaction summary, replacing older messages with a summary.
void apply_compaction_summary(std::vector<ChatMessage>& history,
                               size_t start, size_t compact_end,
                               const std::string& summary);

// ── Context building ─────────────────────────────────────────────

/// Build context preamble by searching memory for relevant entries.
std::string build_context(Memory* mem, const std::string& user_msg,
                          double min_relevance_score);

// ── Tool call parsing ────────────────────────────────────────────

/// Convert a tool registry to OpenAI function-calling format.
std::vector<nlohmann::json> tools_to_openai_format(
    const std::vector<std::unique_ptr<Tool>>& tools_registry);

/// Find a tool by name in the registry.
Tool* find_tool(const std::vector<std::unique_ptr<Tool>>& tools, const std::string& name);

/// Extract a short hint from tool call arguments for progress display.
std::string truncate_tool_args_for_progress(const std::string& name,
                                             const nlohmann::json& args,
                                             size_t max_len);

/// Parse tool calls from a JSON value (supports various API formats).
std::vector<ParsedToolCall> parse_tool_calls_from_json_value(const nlohmann::json& value);

/// Parse XML-style tool calls in <tool_call> bodies.
std::optional<std::vector<ParsedToolCall>> parse_xml_tool_calls(const std::string& xml_content);

/// Parse MiniMax-style XML tool calls with invoke/parameter tags.
std::optional<std::pair<std::string, std::vector<ParsedToolCall>>>
parse_minimax_invoke_calls(const std::string& response);

/// Extract JSON values from a string (for tool call parsing).
std::vector<nlohmann::json> extract_json_values(const std::string& input);

/// Find the end position of a JSON object by tracking balanced braces.
std::optional<size_t> find_json_end(const std::string& input);

/// Truncate a string with ellipsis.
std::string truncate_with_ellipsis(const std::string& s, size_t max_len);

// ── Parse tool responses ─────────────────────────────────────────

/// Parse a complete LLM response text for prompt-guided tool calls.
/// Handles multiple XML tag formats (<tool_call>, <toolcall>, <invoke>, etc.),
/// JSON blocks, and MiniMax invoke tags.
std::pair<std::string, std::vector<ParsedToolCall>>
parse_tool_response(const std::string& response_text,
                    const std::vector<std::unique_ptr<Tool>>& known_tools);

} // namespace agent
} // namespace zeroclaw
