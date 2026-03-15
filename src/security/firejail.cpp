#include "firejail.hpp"
#include <cstdlib>

namespace zeroclaw {
namespace security {

std::unique_ptr<FirejailSandbox> FirejailSandbox::create() {
    if (is_installed()) {
        return std::make_unique<FirejailSandbox>();
    }
    return nullptr;
}

std::unique_ptr<FirejailSandbox> FirejailSandbox::probe() {
    return create();
}

bool FirejailSandbox::is_installed() {
#ifdef _WIN32
    return false; // firejail not available on Windows
#else
    return std::system("firejail --version >/dev/null 2>&1") == 0;
#endif
}

bool FirejailSandbox::wrap_command(std::string& program, std::vector<std::string>& args) {
    std::string original_program = program;
    std::vector<std::string> original_args = args;

    program = "firejail";
    args.clear();
    args.push_back("--private=home");
    args.push_back("--private-dev");
    args.push_back("--nosound");
    args.push_back("--no3d");
    args.push_back("--novideo");
    args.push_back("--nowheel");
    args.push_back("--notv");
    args.push_back("--noprofile");
    args.push_back("--quiet");
    args.push_back(original_program);
    for (const auto& arg : original_args) {
        args.push_back(arg);
    }

    return true;
}

bool FirejailSandbox::is_available() const {
    return is_installed();
}

} // namespace security
} // namespace zeroclaw
