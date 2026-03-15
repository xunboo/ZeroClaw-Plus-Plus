#pragma once

/// Providers module — aggregates all provider implementations
/// and exposes the provider factory for creating providers by name.

#include "providers/traits.hpp"
#include "providers/openai.hpp"
#include "providers/anthropic.hpp"
#include "providers/other_providers.hpp"

#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace zeroclaw {
namespace providers {

/// Provider metadata for the factory registry
struct ProviderInfo {
    std::string name;
    std::vector<std::string> aliases;
};

/// List all registered providers and their aliases
std::vector<ProviderInfo> list_providers();

/// Create a provider by name, with optional credentials and base URL
std::unique_ptr<Provider> create_provider(
    const std::string& name,
    const std::optional<std::string>& api_key = std::nullopt,
    const std::optional<std::string>& base_url = std::nullopt);

/// Create a provider wrapped in ReliableProvider for retry/resilience
std::unique_ptr<Provider> create_resilient_provider(
    const std::string& name,
    const std::optional<std::string>& api_key = std::nullopt,
    const std::optional<std::string>& base_url = std::nullopt,
    int max_retries = 3);

} // namespace providers
} // namespace zeroclaw
