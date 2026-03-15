#pragma once

/// Domain matcher for OTP-gated domain detection.
/// Matches domain patterns with wildcard support and category presets.

#include <string>
#include <vector>
#include <set>
#include <stdexcept>

namespace zeroclaw {
namespace security {

// ── Built-in domain category constants ───────────────────────────

inline const std::vector<std::string> BANKING_DOMAINS = {
    "*.chase.com", "*.bankofamerica.com", "*.wellsfargo.com",
    "*.fidelity.com", "*.schwab.com", "*.venmo.com",
    "*.paypal.com", "*.robinhood.com", "*.coinbase.com"
};

inline const std::vector<std::string> MEDICAL_DOMAINS = {
    "*.mychart.com", "*.epic.com", "*.patient.portal.*", "*.healthrecords.*"
};

inline const std::vector<std::string> GOVERNMENT_DOMAINS = {
    "*.ssa.gov", "*.irs.gov", "*.login.gov", "*.id.me"
};

inline const std::vector<std::string> IDENTITY_PROVIDER_DOMAINS = {
    "accounts.google.com", "login.microsoftonline.com", "appleid.apple.com"
};

struct DomainCategory {
    std::string name;
    const std::vector<std::string>* domains;
};

inline const std::vector<DomainCategory> DOMAIN_CATEGORIES = {
    {"banking", &BANKING_DOMAINS},
    {"medical", &MEDICAL_DOMAINS},
    {"government", &GOVERNMENT_DOMAINS},
    {"identity_providers", &IDENTITY_PROVIDER_DOMAINS}
};

// ── Helper functions ─────────────────────────────────────────────

/// Normalize a domain (strip protocol, path, port, etc.)
std::string normalize_domain(const std::string& raw);

/// Normalize and validate a domain pattern
std::string normalize_pattern(const std::string& raw);

/// Check if a domain matches a wildcard pattern
bool domain_matches_pattern(const std::string& pattern, const std::string& domain);

/// Wildcard matching on byte sequences
bool wildcard_match(const std::string& pattern, const std::string& value);

// ── DomainMatcher ────────────────────────────────────────────────

class DomainMatcher {
public:
    DomainMatcher() = default;

    /// Create a new matcher from explicit domain patterns and category names.
    /// Throws std::runtime_error on invalid patterns or unknown categories.
    DomainMatcher(const std::vector<std::string>& gated_domains,
                  const std::vector<std::string>& categories);

    /// Get the list of patterns
    const std::vector<std::string>& patterns() const { return patterns_; }

    /// Check if a domain is gated (matches any pattern)
    bool is_gated(const std::string& domain) const;

    /// Expand category names into domain pattern lists.
    /// Throws std::runtime_error on unknown categories.
    static std::vector<std::string> expand_categories(const std::vector<std::string>& categories);

    /// Validate a single domain pattern (throws on invalid)
    static void validate_pattern(const std::string& pattern);

private:
    std::vector<std::string> patterns_;
};

} // namespace security
} // namespace zeroclaw
