#include "runtime.hpp"
#include <cstdlib>
#include <array>

namespace zeroclaw {
namespace runtime {

// ── NativeRuntime ────────────────────────────────────────────────

CommandResult NativeRuntime::execute(const std::string& command,
                                      const std::string& /*working_dir*/,
                                      int /*timeout_secs*/) {
    CommandResult result;
    std::string output;

#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) {
        result.exit_code = -1;
        result.stderr_output = "Failed to execute command";
        return result;
    }

    std::array<char, 4096> buffer;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

#ifdef _WIN32
    result.exit_code = _pclose(pipe);
#else
    int status = pclose(pipe);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif

    result.stdout_output = output;
    return result;
}

// ── DockerRuntime ────────────────────────────────────────────────

DockerRuntime::DockerRuntime(const std::string& image,
                              const std::vector<std::string>& volumes,
                              const std::optional<std::string>& network)
    : image_(image), volumes_(volumes), network_(network) {}

bool DockerRuntime::is_available() const {
    // Check if Docker is installed and running
    int result = std::system("docker info > /dev/null 2>&1");
    return result == 0;
}

CommandResult DockerRuntime::execute(const std::string& command,
                                      const std::string& working_dir,
                                      int timeout_secs) {
    std::string docker_cmd = "docker run --rm";
    for (const auto& vol : volumes_) {
        docker_cmd += " -v " + vol;
    }
    if (network_.has_value()) {
        docker_cmd += " --network " + *network_;
    }
    if (!working_dir.empty()) {
        docker_cmd += " -w " + working_dir;
    }
    docker_cmd += " " + image_ + " " + command;

    NativeRuntime native;
    return native.execute(docker_cmd, "", timeout_secs);
}

// ── WasmRuntime ──────────────────────────────────────────────────

WasmRuntime::WasmRuntime(const std::string& wasm_runtime)
    : wasm_runtime_(wasm_runtime) {}

bool WasmRuntime::is_available() const {
    std::string check = wasm_runtime_ + " --version > /dev/null 2>&1";
    return std::system(check.c_str()) == 0;
}

CommandResult WasmRuntime::execute(const std::string& command,
                                     const std::string& /*working_dir*/,
                                     int timeout_secs) {
    std::string wasm_cmd = wasm_runtime_ + " run " + command;
    NativeRuntime native;
    return native.execute(wasm_cmd, "", timeout_secs);
}

// ── Factory ──────────────────────────────────────────────────────

std::unique_ptr<RuntimeAdapter> create_runtime(const std::string& runtime_type) {
    if (runtime_type == "docker") return std::make_unique<DockerRuntime>("zeroclaw:latest");
    if (runtime_type == "wasm") return std::make_unique<WasmRuntime>();
    return std::make_unique<NativeRuntime>();
}

} // namespace runtime
} // namespace zeroclaw
