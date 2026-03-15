#include "detect.hpp"
#include "docker.hpp"
#include "bubblewrap.hpp"
#include "firejail.hpp"
#include "landlock.hpp"
#include <iostream>

namespace zeroclaw {
namespace security {

std::shared_ptr<Sandbox> create_sandbox(const SecurityConfig& config) {
    const auto& backend = config.sandbox.backend;

    // If explicitly disabled, return noop
    if (backend == SandboxBackend::None ||
        (config.sandbox.enabled.has_value() && !config.sandbox.enabled.value())) {
        return std::make_shared<NoopSandbox>();
    }

    switch (backend) {
        case SandboxBackend::Landlock: {
            auto sb = LandlockSandbox::create();
            if (sb) return std::shared_ptr<Sandbox>(std::move(sb));
            std::cerr << "[WARN] Landlock requested but not available, "
                         "falling back to application-layer\n";
            return std::make_shared<NoopSandbox>();
        }
        case SandboxBackend::Firejail: {
#ifdef __linux__
            auto sb = FirejailSandbox::create();
            if (sb) return std::shared_ptr<Sandbox>(std::move(sb));
#endif
            std::cerr << "[WARN] Firejail requested but not available, "
                         "falling back to application-layer\n";
            return std::make_shared<NoopSandbox>();
        }
        case SandboxBackend::Bubblewrap: {
#if defined(__linux__) || defined(__APPLE__)
            auto sb = BubblewrapSandbox::create();
            if (sb) return std::shared_ptr<Sandbox>(std::move(sb));
#endif
            std::cerr << "[WARN] Bubblewrap requested but not available, "
                         "falling back to application-layer\n";
            return std::make_shared<NoopSandbox>();
        }
        case SandboxBackend::Docker: {
            auto sb = DockerSandbox::create();
            if (sb) return std::shared_ptr<Sandbox>(std::move(sb));
            std::cerr << "[WARN] Docker requested but not available, "
                         "falling back to application-layer\n";
            return std::make_shared<NoopSandbox>();
        }
        case SandboxBackend::Auto:
        case SandboxBackend::None:
        default:
            return detect_best_sandbox();
    }
}

std::shared_ptr<Sandbox> detect_best_sandbox() {
#ifdef __linux__
    // Try Landlock first (native, no dependencies)
    auto landlock = LandlockSandbox::probe();
    if (landlock) {
        std::cerr << "[INFO] Landlock sandbox enabled (Linux kernel 5.13+)\n";
        return std::shared_ptr<Sandbox>(std::move(landlock));
    }

    // Try Firejail second (user-space tool)
    auto firejail = FirejailSandbox::probe();
    if (firejail) {
        std::cerr << "[INFO] Firejail sandbox enabled\n";
        return std::shared_ptr<Sandbox>(std::move(firejail));
    }
#endif

#ifdef __APPLE__
    // Try Bubblewrap on macOS
    auto bwrap = BubblewrapSandbox::probe();
    if (bwrap) {
        std::cerr << "[INFO] Bubblewrap sandbox enabled\n";
        return std::shared_ptr<Sandbox>(std::move(bwrap));
    }
#endif

    // Docker works everywhere if installed
    auto docker = DockerSandbox::probe();
    if (docker) {
        std::cerr << "[INFO] Docker sandbox enabled\n";
        return std::shared_ptr<Sandbox>(std::move(docker));
    }

    // Fallback: application-layer security only
    std::cerr << "[INFO] No sandbox backend available, using application-layer security\n";
    return std::make_shared<NoopSandbox>();
}

} // namespace security
} // namespace zeroclaw
