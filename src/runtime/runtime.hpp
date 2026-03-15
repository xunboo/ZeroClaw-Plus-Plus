#pragma once

/// Runtime module — execution environment abstractions.
/// Provides native, Docker, and WASM runtime adapters.

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace runtime {

/// Command execution result
struct CommandResult {
    int exit_code = 0;
    std::string stdout_output;
    std::string stderr_output;
};

/// Abstract runtime adapter for command execution
class RuntimeAdapter {
public:
    virtual ~RuntimeAdapter() = default;

    /// Runtime name (native, docker, wasm)
    virtual std::string name() const = 0;

    /// Execute a command with optional timeout
    virtual CommandResult execute(const std::string& command,
                                   const std::string& working_dir = "",
                                   int timeout_secs = 120) = 0;

    /// Check if the runtime is available
    virtual bool is_available() const = 0;
};

/// Native runtime — runs commands directly on the host OS
class NativeRuntime : public RuntimeAdapter {
public:
    std::string name() const override { return "native"; }
    CommandResult execute(const std::string& command,
                           const std::string& working_dir = "",
                           int timeout_secs = 120) override;
    bool is_available() const override { return true; }
};

/// Docker runtime — runs commands inside a Docker container
class DockerRuntime : public RuntimeAdapter {
public:
    DockerRuntime(const std::string& image,
                  const std::vector<std::string>& volumes = {},
                  const std::optional<std::string>& network = std::nullopt);

    std::string name() const override { return "docker"; }
    CommandResult execute(const std::string& command,
                           const std::string& working_dir = "",
                           int timeout_secs = 120) override;
    bool is_available() const override;

private:
    std::string image_;
    std::vector<std::string> volumes_;
    std::optional<std::string> network_;
};

/// WASM runtime — sandboxed execution via WebAssembly
class WasmRuntime : public RuntimeAdapter {
public:
    explicit WasmRuntime(const std::string& wasm_runtime = "wasmtime");

    std::string name() const override { return "wasm"; }
    CommandResult execute(const std::string& command,
                           const std::string& working_dir = "",
                           int timeout_secs = 120) override;
    bool is_available() const override;

private:
    std::string wasm_runtime_;
};

/// Create a runtime adapter by name
std::unique_ptr<RuntimeAdapter> create_runtime(const std::string& runtime_type);

} // namespace runtime
} // namespace zeroclaw
