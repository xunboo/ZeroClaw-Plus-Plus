#pragma once

/// Query classifier — matches user messages against rules to select model routes.

#include <string>
#include <vector>
#include <optional>
#include <algorithm>
#include <cctype>

namespace zeroclaw {
namespace agent {

/// A classification rule
struct ClassificationRule {
    std::string hint;
    std::vector<std::string> keywords;
    std::vector<std::string> patterns;
    int priority = 0;
    std::optional<size_t> min_length;
    std::optional<size_t> max_length;
};

/// Configuration for query classification
struct QueryClassificationConfig {
    bool enabled = false;
    std::vector<ClassificationRule> rules;
};

/// Classify a user message against the configured rules and return the
/// matching hint string, if any.
///
/// Returns empty optional when classification is disabled, no rules are configured,
/// or no rule matches the message.
inline std::optional<std::string> classify(const QueryClassificationConfig& config,
                                            const std::string& message) {
    if (!config.enabled || config.rules.empty()) {
        return std::nullopt;
    }

    // Lowercase copy for keyword matching
    std::string lower = message;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    size_t len = message.size();

    // Sort rules by priority (descending)
    auto sorted_rules = config.rules;
    std::sort(sorted_rules.begin(), sorted_rules.end(),
              [](const ClassificationRule& a, const ClassificationRule& b) {
                  return b.priority < a.priority;
              });

    for (const auto& rule : sorted_rules) {
        // Length constraints
        if (rule.min_length.has_value() && len < *rule.min_length) continue;
        if (rule.max_length.has_value() && len > *rule.max_length) continue;

        // Check keywords (case-insensitive)
        bool keyword_hit = false;
        for (const auto& kw : rule.keywords) {
            std::string kw_lower = kw;
            std::transform(kw_lower.begin(), kw_lower.end(), kw_lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (lower.find(kw_lower) != std::string::npos) {
                keyword_hit = true;
                break;
            }
        }

        // Check patterns (case-sensitive)
        bool pattern_hit = false;
        for (const auto& pat : rule.patterns) {
            if (message.find(pat) != std::string::npos) {
                pattern_hit = true;
                break;
            }
        }

        if (keyword_hit || pattern_hit) {
            return rule.hint;
        }
    }

    return std::nullopt;
}

} // namespace agent
} // namespace zeroclaw
