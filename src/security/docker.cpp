#include "docker.hpp"
#include <cstdlib>

namespace zeroclaw {
namespace security {

std::unique_ptr<DockerSandbox> DockerSandbox::create() {
    if (is_installed()) {
        return std::make_unique<DockerSandbox>();
    }
    return nullptr;
}

std::unique_ptr<DockerSandbox> DockerSandbox::create_with_image(const std::string& image) {
    if (is_installed()) {
        return std::make_unique<DockerSandbox>(image);
    }
    return nullptr;
}

std::unique_ptr<DockerSandbox> DockerSandbox::probe() {
    return create();
}

bool DockerSandbox::is_installed() {
#ifdef _WIN32
    return std::system("docker --version >NUL 2>&1") == 0;
#else
    return std::system("docker --version >/dev/null 2>&1") == 0;
#endif
}

bool DockerSandbox::wrap_command(std::string& program, std::vector<std::string>& args) {
    // Save original command
    std::string original_program = program;
    std::vector<std::string> original_args = args;

    // Build docker command
    program = "docker";
    args.clear();
    args.push_back("run");
    args.push_back("--rm");
    args.push_back("--memory");
    args.push_back("512m");
    args.push_back("--cpus");
    args.push_back("1.0");
    args.push_back("--network");
    args.push_back("none");
    args.push_back(image_);
    args.push_back(original_program);
    for (const auto& arg : original_args) {
        args.push_back(arg);
    }

    return true;
}

bool DockerSandbox::is_available() const {
    return is_installed();
}

} // namespace security
} // namespace zeroclaw
