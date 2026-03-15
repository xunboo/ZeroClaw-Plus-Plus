#include "bubblewrap.hpp"
#include <cstdlib>

namespace zeroclaw {
namespace security {

std::unique_ptr<BubblewrapSandbox> BubblewrapSandbox::create() {
    if (is_installed()) {
        return std::make_unique<BubblewrapSandbox>();
    }
    return nullptr;
}

std::unique_ptr<BubblewrapSandbox> BubblewrapSandbox::probe() {
    return create();
}

bool BubblewrapSandbox::is_installed() {
#ifdef _WIN32
    return false; // bwrap not available on Windows
#else
    return std::system("bwrap --version >/dev/null 2>&1") == 0;
#endif
}

bool BubblewrapSandbox::wrap_command(std::string& program, std::vector<std::string>& args) {
    std::string original_program = program;
    std::vector<std::string> original_args = args;

    program = "bwrap";
    args.clear();
    args.push_back("--ro-bind");
    args.push_back("/usr");
    args.push_back("/usr");
    args.push_back("--dev");
    args.push_back("/dev");
    args.push_back("--proc");
    args.push_back("/proc");
    args.push_back("--bind");
    args.push_back("/tmp");
    args.push_back("/tmp");
    args.push_back("--unshare-all");
    args.push_back("--die-with-parent");
    args.push_back(original_program);
    for (const auto& arg : original_args) {
        args.push_back(arg);
    }

    return true;
}

bool BubblewrapSandbox::is_available() const {
    return is_installed();
}

} // namespace security
} // namespace zeroclaw
