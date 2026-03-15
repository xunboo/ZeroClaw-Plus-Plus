#include "landlock.hpp"

namespace zeroclaw {
namespace security {

// Landlock is Linux-only. On other platforms, this is always unavailable.

std::unique_ptr<LandlockSandbox> LandlockSandbox::create() {
    // Landlock is only supported on Linux with kernel 5.13+
    // On other platforms, always return nullptr
#ifdef __linux__
    // TODO: Implement actual Landlock detection and ruleset creation
    // For now, return nullptr (no Landlock support in C++ build)
    return nullptr;
#else
    return nullptr;
#endif
}

std::unique_ptr<LandlockSandbox> LandlockSandbox::create_with_workspace(
    const std::optional<std::filesystem::path>& /*workspace_dir*/) {
#ifdef __linux__
    return nullptr; // TODO: implement
#else
    return nullptr;
#endif
}

std::unique_ptr<LandlockSandbox> LandlockSandbox::probe() {
    return create();
}

bool LandlockSandbox::wrap_command(std::string& /*program*/, std::vector<std::string>& /*args*/) {
    // Landlock restrictions are applied at process level, not per-command
    // On non-Linux or without Landlock support, this is a no-op failure
    return false;
}

bool LandlockSandbox::is_available() const {
#ifdef __linux__
    return false; // TODO: actual availability check
#else
    return false;
#endif
}

std::string LandlockSandbox::description() const {
#ifdef __linux__
    return "Linux kernel LSM sandboxing (filesystem access control)";
#else
    return "Linux kernel LSM sandboxing (not available on this platform)";
#endif
}

} // namespace security
} // namespace zeroclaw
