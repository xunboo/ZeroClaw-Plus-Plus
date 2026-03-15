#include "loop_.hpp"
#include <regex>
#include <sstream>
#include <cmath>

namespace zeroclaw {
namespace agent {

// ── Credential scrubbing ─────────────────────────────────────────

std::string scrub_credentials(const std::string& input) {
    // Pattern: key=value or key: value where key matches credential-related names
    static const std::regex kv_regex(
        R"REGEX((?i)(token|api[_-]?key|password|secret|user[_-]?key|bearer|credential)["']?\s*[:=]\s*(?:"([^"]{8,})"|'([^']{8,})'|([a-zA-Z0-9_\-\.]{8,})))REGEX",
        std::regex::icase);

    std::string result;
    std::sregex_iterator it(input.begin(), input.end(), kv_regex);
    std::sregex_iterator end;
    size_t last_end = 0;

    for (; it != end; ++it) {
        auto& match = *it;
        result += input.substr(last_end, static_cast<size_t>(match.position()) - last_end);

        std::string key = match[1].str();
        std::string val;
        if (match[2].matched) val = match[2].str();
        else if (match[3].matched) val = match[3].str();
        else if (match[4].matched) val = match[4].str();

        // Preserve first 4 chars for context
        std::string prefix = val.size() > 4 ? val.substr(0, 4) : "";

        std::string full = match[0].str();
        if (full.find(':') != std::string::npos) {
            if (full.find('"') != std::string::npos) {
                result += "\"" + key + "\": \"" + prefix + "*[REDACTED]\"";
            } else {
                result += key + ": " + prefix + "*[REDACTED]";
            }
        } else if (full.find('=') != std::string::npos) {
            if (full.find('"') != std::string::npos) {
                result += key + "=\"" + prefix + "*[REDACTED]\"";
            } else {
                result += key + "=" + prefix + "*[REDACTED]";
            }
        } else {
            result += key + ": " + prefix + "*[REDACTED]";
        }

        last_end = static_cast<size_t>(match.position()) + match.length();
    }
    result += input.substr(last_end);
    return result;
}

// ── History management ───────────────────────────────────────────

void trim_history(std::vector<ChatMessage>& history, size_t max_history) {
    bool has_system = !history.empty() && history[0].role == "system";
    size_t non_system_count = has_system ? history.size() - 1 : history.size();

    if (non_system_count <= max_history) return;

    size_t start = has_system ? 1 : 0;
    size_t to_remove = non_system_count - max_history;
    history.erase(history.begin() + static_cast<ptrdiff_t>(start),
                  history.begin() + static_cast<ptrdiff_t>(start + to_remove));
}

std::string build_compaction_transcript(const std::vector<ChatMessage>& messages) {
    std::ostringstream oss;
    for (const auto& msg : messages) {
        std::string role = msg.role;
        // Uppercase
        std::transform(role.begin(), role.end(), role.begin(), ::toupper);
        oss << role << ": " << msg.content << "\n";
    }

    std::string transcript = oss.str();
    if (transcript.size() > COMPACTION_MAX_SOURCE_CHARS) {
        return truncate_with_ellipsis(transcript, COMPACTION_MAX_SOURCE_CHARS);
    }
    return transcript;
}

void apply_compaction_summary(std::vector<ChatMessage>& history,
                               size_t start, size_t compact_end,
                               const std::string& summary) {
    auto summary_msg = ChatMessage::assistant("[Compaction summary]\n" + summary);
    history.erase(history.begin() + static_cast<ptrdiff_t>(start),
                  history.begin() + static_cast<ptrdiff_t>(compact_end));
    history.insert(history.begin() + static_cast<ptrdiff_t>(start), summary_msg);
}

// ── Context building ─────────────────────────────────────────────

std::string build_context(Memory* mem, const std::string& user_msg,
                          double min_relevance_score) {
    if (!mem) return "";

    auto entries = mem->recall(user_msg, 5, std::nullopt);
    if (entries.empty()) return "";

    std::ostringstream oss;
    oss << "[Memory context]\n";
    bool has_entry = false;

    for (const auto& entry : entries) {
        if (entry.score.has_value() && *entry.score < min_relevance_score) continue;
        if (is_assistant_autosave_key(entry.key)) continue;
        oss << "- " << entry.key << ": " << entry.content << "\n";
        has_entry = true;
    }

    if (!has_entry) return "";

    oss << "\n";
    return oss.str();
}

// ── Tool helpers ─────────────────────────────────────────────────

std::vector<nlohmann::json> tools_to_openai_format(
    const std::vector<std::unique_ptr<Tool>>& tools_registry) {
    std::vector<nlohmann::json> result;
    for (const auto& tool : tools_registry) {
        result.push_back({
            {"type", "function"},
            {"function", {
                {"name", tool->name()},
                {"description", tool->description()},
                {"parameters", tool->parameters_schema()}
            }}
        });
    }
    return result;
}

Tool* find_tool(const std::vector<std::unique_ptr<Tool>>& tools, const std::string& name) {
    for (const auto& tool : tools) {
        if (tool->name() == name) return tool.get();
    }
    return nullptr;
}

std::string truncate_tool_args_for_progress(const std::string& name,
                                             const nlohmann::json& args,
                                             size_t max_len) {
    std::string hint;
    if (name == "shell") {
        if (args.contains("command") && args["command"].is_string()) {
            hint = args["command"].get<std::string>();
        }
    } else if (name == "file_read" || name == "file_write") {
        if (args.contains("path") && args["path"].is_string()) {
            hint = args["path"].get<std::string>();
        }
    } else {
        if (args.contains("action") && args["action"].is_string()) {
            hint = args["action"].get<std::string>();
        } else if (args.contains("query") && args["query"].is_string()) {
            hint = args["query"].get<std::string>();
        }
    }

    return truncate_with_ellipsis(hint, max_len);
}

std::string truncate_with_ellipsis(const std::string& s, size_t max_len) {
    if (s.size() <= max_len) return s;
    if (max_len <= 3) return s.substr(0, max_len);
    return s.substr(0, max_len - 3) + "...";
}

// ── JSON extraction ──────────────────────────────────────────────

std::vector<nlohmann::json> extract_json_values(const std::string& input) {
    std::vector<nlohmann::json> values;
    std::string trimmed = input;

    // Trim whitespace
    size_t s = trimmed.find_first_not_of(" \t\n\r");
    size_t e = trimmed.find_last_not_of(" \t\n\r");
    if (s == std::string::npos) return values;
    trimmed = trimmed.substr(s, e - s + 1);

    // Try parsing entire string as JSON
    try {
        auto value = nlohmann::json::parse(trimmed);
        values.push_back(value);
        return values;
    } catch (...) {}

    // Scan for JSON objects/arrays
    for (size_t i = 0; i < trimmed.size(); ++i) {
        char ch = trimmed[i];
        if (ch != '{' && ch != '[') continue;

        // Try to parse JSON starting at this position
        try {
            auto value = nlohmann::json::parse(
                trimmed.begin() + static_cast<ptrdiff_t>(i),
                trimmed.end(),
                nullptr, false);
            if (!value.is_discarded()) {
                values.push_back(value);
                // Skip past this JSON value
                auto dump = value.dump();
                // Rough skip — find the matching brace
                auto end_pos = find_json_end(trimmed.substr(i));
                if (end_pos.has_value()) {
                    i += *end_pos - 1;
                }
            }
        } catch (...) {}
    }

    return values;
}

std::optional<size_t> find_json_end(const std::string& input) {
    // Trim leading whitespace
    size_t offset = 0;
    while (offset < input.size() && (input[offset] == ' ' || input[offset] == '\t' ||
           input[offset] == '\n' || input[offset] == '\r')) {
        ++offset;
    }

    if (offset >= input.size() || input[offset] != '{') return std::nullopt;

    int depth = 0;
    bool in_string = false;
    bool escape_next = false;

    for (size_t i = offset; i < input.size(); ++i) {
        char ch = input[i];
        if (escape_next) {
            escape_next = false;
            continue;
        }
        if (ch == '\\' && in_string) {
            escape_next = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) continue;

        if (ch == '{') ++depth;
        else if (ch == '}') {
            --depth;
            if (depth == 0) return i + 1;
        }
    }

    return std::nullopt;
}

// ── Tool call parsing ────────────────────────────────────────────

static bool is_xml_meta_tag(const std::string& tag) {
    std::string lower = tag;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "tool_call" || lower == "toolcall" || lower == "tool-call" ||
           lower == "invoke" || lower == "thinking" || lower == "thought" ||
           lower == "analysis" || lower == "reasoning" || lower == "reflection";
}

std::optional<std::vector<ParsedToolCall>> parse_xml_tool_calls(const std::string& xml_content) {
    std::string trimmed = xml_content;
    size_t s = trimmed.find_first_not_of(" \t\n\r");
    size_t e = trimmed.find_last_not_of(" \t\n\r");
    if (s == std::string::npos) return std::nullopt;
    trimmed = trimmed.substr(s, e - s + 1);

    if (trimmed.empty() || trimmed[0] != '<' || trimmed.find('>') == std::string::npos) {
        return std::nullopt;
    }

    std::vector<ParsedToolCall> calls;

    // Simple XML pair extraction: find <tag>...</tag> patterns
    std::regex tag_re("<([a-zA-Z_][a-zA-Z0-9_-]*)>");
    std::sregex_iterator it(trimmed.begin(), trimmed.end(), tag_re);
    std::sregex_iterator end_it;

    size_t search_start = 0;
    for (; it != end_it; ++it) {
        auto& match = *it;
        std::string tag_name = match[1].str();
        if (is_xml_meta_tag(tag_name)) continue;

        size_t open_end = static_cast<size_t>(match.position()) + match.length();
        std::string close_tag = "</" + tag_name + ">";
        auto close_pos = trimmed.find(close_tag, open_end);
        if (close_pos == std::string::npos) continue;

        std::string inner = trimmed.substr(open_end, close_pos - open_end);
        // Trim inner
        size_t is = inner.find_first_not_of(" \t\n\r");
        size_t ie = inner.find_last_not_of(" \t\n\r");
        if (is != std::string::npos) {
            inner = inner.substr(is, ie - is + 1);
        } else {
            continue;
        }

        nlohmann::json args = nlohmann::json::object();

        // Try to parse inner as JSON first
        auto json_values = extract_json_values(inner);
        if (!json_values.empty()) {
            if (json_values[0].is_object()) {
                args = json_values[0];
            } else {
                args["value"] = json_values[0];
            }
        } else {
            // Try nested XML pairs as arguments
            std::sregex_iterator inner_it(inner.begin(), inner.end(), tag_re);
            bool found_inner = false;
            for (; inner_it != end_it; ++inner_it) {
                auto& inner_match = *inner_it;
                std::string key = inner_match[1].str();
                if (is_xml_meta_tag(key)) continue;

                size_t key_end = static_cast<size_t>(inner_match.position()) + inner_match.length();
                std::string key_close = "</" + key + ">";
                auto key_close_pos = inner.find(key_close, key_end);
                if (key_close_pos == std::string::npos) continue;

                std::string value = inner.substr(key_end, key_close_pos - key_end);
                // Trim value
                size_t vs = value.find_first_not_of(" \t\n\r");
                size_t ve = value.find_last_not_of(" \t\n\r");
                if (vs != std::string::npos) {
                    value = value.substr(vs, ve - vs + 1);
                }
                if (!value.empty()) {
                    args[key] = value;
                    found_inner = true;
                }
            }

            if (!found_inner) {
                args["content"] = inner;
            }
        }

        calls.push_back({tag_name, args, std::nullopt});
    }

    if (calls.empty()) return std::nullopt;
    return calls;
}

std::vector<ParsedToolCall> parse_tool_calls_from_json_value(const nlohmann::json& value) {
    std::vector<ParsedToolCall> calls;

    // Check for tool_calls array
    if (value.contains("tool_calls") && value["tool_calls"].is_array()) {
        for (const auto& call : value["tool_calls"]) {
            std::string name;
            nlohmann::json args = nlohmann::json::object();
            std::optional<std::string> tc_id;

            if (call.contains("function") && call["function"].is_object()) {
                auto& func = call["function"];
                if (func.contains("name") && func["name"].is_string()) {
                    name = func["name"].get<std::string>();
                }
                if (func.contains("arguments")) {
                    if (func["arguments"].is_string()) {
                        try { args = nlohmann::json::parse(func["arguments"].get<std::string>()); }
                        catch (...) {}
                    } else {
                        args = func["arguments"];
                    }
                }
            } else if (call.contains("name") && call["name"].is_string()) {
                name = call["name"].get<std::string>();
                if (call.contains("arguments")) {
                    if (call["arguments"].is_string()) {
                        try { args = nlohmann::json::parse(call["arguments"].get<std::string>()); }
                        catch (...) {}
                    } else {
                        args = call["arguments"];
                    }
                }
            }

            // Extract tool_call_id
            if (call.contains("id") && call["id"].is_string()) {
                tc_id = call["id"].get<std::string>();
            }

            if (!name.empty()) {
                calls.push_back({name, args, tc_id});
            }
        }
        if (!calls.empty()) return calls;
    }

    // Check if value is an array of tool calls
    if (value.is_array()) {
        for (const auto& item : value) {
            auto sub_calls = parse_tool_calls_from_json_value(item);
            calls.insert(calls.end(), sub_calls.begin(), sub_calls.end());
        }
        return calls;
    }

    // Single tool call object
    if (value.contains("name") && value["name"].is_string()) {
        std::string name = value["name"].get<std::string>();
        nlohmann::json args = nlohmann::json::object();
        if (value.contains("arguments")) {
            if (value["arguments"].is_string()) {
                try { args = nlohmann::json::parse(value["arguments"].get<std::string>()); }
                catch (...) {}
            } else {
                args = value["arguments"];
            }
        }
        std::optional<std::string> tc_id;
        if (value.contains("id") && value["id"].is_string()) {
            tc_id = value["id"].get<std::string>();
        }
        if (!name.empty()) {
            calls.push_back({name, args, tc_id});
        }
    }

    return calls;
}

std::pair<std::string, std::vector<ParsedToolCall>>
parse_tool_response(const std::string& response_text,
                    const std::vector<std::unique_ptr<Tool>>& known_tools) {
    // First, try XML-style tool calls
    static const std::vector<std::string> open_tags = {
        "<tool_call>", "<toolcall>", "<tool-call>",
        "<invoke>", "<minimax:tool_call>", "<minimax:toolcall>"
    };

    for (const auto& tag : open_tags) {
        if (response_text.find(tag) != std::string::npos) {
            // Try XML parsing
            auto [text, calls] = XmlToolDispatcher::parse_xml_tool_calls(response_text);
            if (!calls.empty()) {
                return {text, calls};
            }
        }
    }

    // Try JSON-based tool call extraction
    auto json_values = extract_json_values(response_text);
    for (const auto& val : json_values) {
        auto calls = parse_tool_calls_from_json_value(val);
        if (!calls.empty()) {
            return {"", calls};
        }
    }

    // No tool calls found
    return {response_text, {}};
}

} // namespace agent
} // namespace zeroclaw
