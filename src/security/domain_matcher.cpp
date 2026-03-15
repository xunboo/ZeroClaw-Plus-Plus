#include "domain_matcher.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace zeroclaw {
namespace security {

// ── Helper functions ─────────────────────────────────────────────

std::string normalize_domain(const std::string& raw) {
    std::string domain = raw;
    // Trim
    size_t start = domain.find_first_not_of(" \t\n\r");
    size_t end = domain.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    domain = domain.substr(start, end - start + 1);

    // To lowercase
    std::transform(domain.begin(), domain.end(), domain.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (domain.empty()) return "";

    // Strip protocol
    auto proto_pos = domain.find("://");
    if (proto_pos != std::string::npos) {
        domain = domain.substr(proto_pos + 3);
    }

    // Strip path/query/fragment
    auto sep_pos = domain.find_first_of("/?#");
    if (sep_pos != std::string::npos) {
        domain = domain.substr(0, sep_pos);
    }

    // Strip user info
    auto at_pos = domain.rfind('@');
    if (at_pos != std::string::npos) {
        domain = domain.substr(at_pos + 1);
    }

    // Strip port
    auto colon_pos = domain.find(':');
    if (colon_pos != std::string::npos) {
        domain = domain.substr(0, colon_pos);
    }

    // Strip trailing dots
    while (!domain.empty() && domain.back() == '.') {
        domain.pop_back();
    }

    return domain.empty() ? "" : domain;
}

std::string normalize_pattern(const std::string& raw) {
    std::string pattern = raw;
    // Trim
    size_t start = pattern.find_first_not_of(" \t\n\r");
    size_t end = pattern.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) {
        throw std::runtime_error("Domain pattern must not be empty");
    }
    pattern = pattern.substr(start, end - start + 1);

    // To lowercase
    std::transform(pattern.begin(), pattern.end(), pattern.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (pattern.empty()) {
        throw std::runtime_error("Domain pattern must not be empty");
    }
    if (pattern == "*") return pattern;

    if (pattern.front() == '.' || pattern.back() == '.') {
        throw std::runtime_error("Domain pattern '" + raw + "' must not start or end with '.'");
    }
    if (pattern.find("..") != std::string::npos) {
        throw std::runtime_error("Domain pattern '" + raw + "' must not contain consecutive dots");
    }
    if (pattern.find("**") != std::string::npos) {
        throw std::runtime_error("Domain pattern '" + raw + "' must not contain consecutive '*'");
    }

    for (char c : pattern) {
        if (!(std::islower(static_cast<unsigned char>(c)) ||
              std::isdigit(static_cast<unsigned char>(c)) ||
              c == '.' || c == '-' || c == '*')) {
            throw std::runtime_error("Domain pattern '" + raw +
                                     "' contains invalid characters; allowed: a-z, 0-9, '.', '-', '*'");
        }
    }

    // Check for empty labels
    size_t pos = 0;
    while (pos < pattern.size()) {
        size_t dot = pattern.find('.', pos);
        std::string label = (dot == std::string::npos) ? pattern.substr(pos)
                                                        : pattern.substr(pos, dot - pos);
        if (label.empty()) {
            throw std::runtime_error("Domain pattern '" + raw + "' contains an empty label");
        }
        if (dot == std::string::npos) break;
        pos = dot + 1;
    }

    if (pattern.substr(0, 2) == "*." && pattern.size() <= 2) {
        throw std::runtime_error("Domain pattern '" + raw + "' is incomplete");
    }

    return pattern;
}

bool wildcard_match(const std::string& pattern, const std::string& value) {
    size_t p = 0, v = 0;
    size_t star_idx = std::string::npos;
    size_t match_idx = 0;

    while (v < value.size()) {
        if (p < pattern.size() && pattern[p] == value[v]) {
            ++p; ++v;
            continue;
        }
        if (p < pattern.size() && pattern[p] == '*') {
            star_idx = p;
            ++p;
            match_idx = v;
            continue;
        }
        if (star_idx != std::string::npos) {
            p = star_idx + 1;
            ++match_idx;
            v = match_idx;
            continue;
        }
        return false;
    }

    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

bool domain_matches_pattern(const std::string& pattern, const std::string& domain) {
    if (pattern == "*") return true;
    if (pattern.find('*') == std::string::npos) return pattern == domain;
    return wildcard_match(pattern, domain);
}

// ── DomainMatcher ────────────────────────────────────────────────

DomainMatcher::DomainMatcher(const std::vector<std::string>& gated_domains,
                             const std::vector<std::string>& categories) {
    std::set<std::string> pattern_set;

    for (const auto& domain : gated_domains) {
        pattern_set.insert(normalize_pattern(domain));
    }

    auto expanded = expand_categories(categories);
    for (const auto& domain : expanded) {
        pattern_set.insert(domain);
    }

    patterns_.assign(pattern_set.begin(), pattern_set.end());
}

bool DomainMatcher::is_gated(const std::string& domain) const {
    auto normalized = normalize_domain(domain);
    if (normalized.empty()) return false;

    for (const auto& pattern : patterns_) {
        if (domain_matches_pattern(pattern, normalized)) return true;
    }
    return false;
}

std::vector<std::string> DomainMatcher::expand_categories(const std::vector<std::string>& categories) {
    std::vector<std::string> expanded;
    for (const auto& category : categories) {
        std::string normalized = category;
        // Trim and lowercase
        size_t start = normalized.find_first_not_of(" \t\n\r");
        size_t end = normalized.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) continue;
        normalized = normalized.substr(start, end - start + 1);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        bool found = false;
        for (const auto& cat : DOMAIN_CATEGORIES) {
            if (cat.name == normalized) {
                for (const auto& d : *cat.domains) {
                    expanded.push_back(d);
                }
                found = true;
                break;
            }
        }
        if (!found) {
            std::string known;
            for (const auto& cat : DOMAIN_CATEGORIES) {
                if (!known.empty()) known += ", ";
                known += cat.name;
            }
            throw std::runtime_error("Unknown OTP domain category '" + category +
                                     "'. Known categories: " + known);
        }
    }
    return expanded;
}

void DomainMatcher::validate_pattern(const std::string& pattern) {
    normalize_pattern(pattern); // throws on invalid
}

} // namespace security
} // namespace zeroclaw
