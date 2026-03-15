#pragma once

/// Security subsystem for policy enforcement, sandboxing, and secret management.
///
/// This module provides the security infrastructure for ZeroClaw++. The core type
/// SecurityPolicy defines autonomy levels, workspace boundaries, and access-control
/// rules enforced across the tool and runtime subsystems.
///
/// PairingGuard implements device pairing for channel authentication, and
/// SecretStore handles encrypted credential storage.
///
/// OS-level isolation is provided through the Sandbox interface defined in traits.hpp,
/// with pluggable backends including Docker, Firejail, Bubblewrap, and Landlock.
/// The create_sandbox() function selects the best available backend at runtime.
/// AuditLogger records security-relevant events for forensic review.

// Core types
#include "policy.hpp"
#include "traits.hpp"

// Audit logging
#include "audit.hpp"

// Domain matching
#include "domain_matcher.hpp"

// Emergency stop
#include "estop.hpp"

// OTP authentication
#include "otp.hpp"

// Pairing
#include "pairing.hpp"

// Secret storage
#include "secrets.hpp"

// Sandbox detection
#include "detect.hpp"

// Sandbox backends
#include "docker.hpp"
#include "bubblewrap.hpp"
#include "firejail.hpp"
#include "landlock.hpp"

namespace zeroclaw {
namespace security {

// Re-export redact function (defined in policy.hpp)
// Already available via policy.hpp include

} // namespace security
} // namespace zeroclaw
