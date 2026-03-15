#pragma once

/// Tool traits — core types for agent-callable capabilities.
///
/// This module defines the Tool interface that all tools implement, along with
/// the ToolResult and ToolSpec types used for LLM function calling.

#include <string>
#include <optional>
#include <memory>
#include "nlohmann/json.hpp"

namespace zeroclaw {

/// Result of a tool execution
struct ToolResult {
    bool success = false;
    std::string output;
    std::optional<std::string> error;

    /// Create a success result
    static ToolResult ok(const std::string& output) {
        return {true, output, std::nullopt};
    }

    /// Create a failure result
    static ToolResult fail(const std::string& error_msg) {
        return {false, "", error_msg};
    }

    /// JSON serialization
    nlohmann::json to_json() const {
        nlohmann::json j;
        j["success"] = success;
        j["output"] = output;
        if (error.has_value()) {
            j["error"] = error.value();
        }
        return j;
    }

    /// JSON deserialization
    static ToolResult from_json(const nlohmann::json& j) {
        ToolResult r;
        r.success = j.value("success", false);
        r.output = j.value("output", "");
        if (j.contains("error") && !j["error"].is_null()) {
            r.error = j["error"].get<std::string>();
        }
        return r;
    }
};

/// Description of a tool for the LLM
struct ToolSpec {
    std::string name;
    std::string description;
    nlohmann::json parameters;

    /// JSON serialization
    nlohmann::json to_json() const {
        return {
            {"name", name},
            {"description", description},
            {"parameters", parameters}
        };
    }

    /// JSON deserialization
    static ToolSpec from_json(const nlohmann::json& j) {
        return {
            j.value("name", ""),
            j.value("description", ""),
            j.value("parameters", nlohmann::json::object())
        };
    }
};

/// Core tool trait — implement for any capability.
///
/// Tools represent actions the LLM agent can take (shell execution, file I/O,
/// web search, etc.). Each tool provides a name, description, and JSON schema
/// for parameters, plus an execute method that performs the action.
class Tool {
public:
    virtual ~Tool() = default;

    /// Tool name (used in LLM function calling)
    virtual std::string name() const = 0;

    /// Human-readable description
    virtual std::string description() const = 0;

    /// JSON schema for parameters
    virtual nlohmann::json parameters_schema() const = 0;

    /// Execute the tool with given arguments.
    /// Returns a ToolResult indicating success/failure and output.
    virtual ToolResult execute(const nlohmann::json& args) = 0;

    /// Get the full spec for LLM registration
    virtual ToolSpec spec() const {
        return {name(), description(), parameters_schema()};
    }
};

} // namespace zeroclaw
