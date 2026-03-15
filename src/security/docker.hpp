#pragma once

/// Docker sandbox (container isolation)

#include "traits.hpp"
#include <string>
#include <vector>

namespace zeroclaw {
namespace security {

/// Docker sandbox backend
class DockerSandbox : public Sandbox {
public:
    DockerSandbox() : image_("alpine:latest") {}
    explicit DockerSandbox(const std::string& image) : image_(image) {}

    /// Create a new Docker sandbox (checks if Docker is installed)
    static std::unique_ptr<DockerSandbox> create();

    /// Create with custom image
    static std::unique_ptr<DockerSandbox> create_with_image(const std::string& image);

    /// Probe if Docker is available (alias for create)
    static std::unique_ptr<DockerSandbox> probe();

    /// Check if Docker is installed
    static bool is_installed();

    // Sandbox interface
    bool wrap_command(std::string& program, std::vector<std::string>& args) override;
    bool is_available() const override;
    std::string name() const override { return "docker"; }
    std::string description() const override {
        return "Docker container isolation (requires docker)";
    }

    const std::string& image() const { return image_; }

private:
    std::string image_;
};

} // namespace security
} // namespace zeroclaw
