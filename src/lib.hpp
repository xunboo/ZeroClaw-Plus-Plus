#pragma once

/// ZeroClaw++ library header — top-level module aggregator.
/// Re-exports all subsystems for convenience.

// Core types and traits
#include "providers/traits.hpp"
#include "channels/traits.hpp"
#include "tools/traits.hpp"

// Module aggregators
#include "agent/agent_module.hpp"
#include "channels_module.hpp"
#include "providers_module.hpp"
#include "tools_module.hpp"

// Supporting modules
#include "identity.hpp"
#include "multimodal.hpp"
#include "migration.hpp"
#include "runtime/runtime.hpp"
#include "tunnel/tunnel.hpp"
#include "skills/skills.hpp"
#include "skillforge/skillforge.hpp"
#include "rag/rag.hpp"
#include "service/service.hpp"
#include "peripherals/peripherals.hpp"

namespace zeroclaw {

/// Library version
constexpr const char* VERSION = "0.1.0";

/// Library name
constexpr const char* NAME = "ZeroClaw++";

} // namespace zeroclaw
