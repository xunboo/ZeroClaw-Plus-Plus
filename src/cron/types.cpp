#include "types.hpp"
#include <algorithm>
#include <cctype>

namespace zeroclaw::cron {

std::string job_type_to_string(JobType type) {
    switch (type) {
        case JobType::Shell: return "shell";
        case JobType::Agent: return "agent";
    }
    return "shell";
}

JobType job_type_from_string(const std::string& str) {
    std::string lower;
    lower.reserve(str.size());
    for (char c : str) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    
    if (lower == "shell") return JobType::Shell;
    if (lower == "agent") return JobType::Agent;
    
    throw std::invalid_argument(
        "Invalid job type '" + str + "'. Expected one of: 'shell', 'agent'"
    );
}

std::string session_target_to_string(SessionTarget target) {
    switch (target) {
        case SessionTarget::Isolated: return "isolated";
        case SessionTarget::Main: return "main";
    }
    return "isolated";
}

SessionTarget session_target_from_string(const std::string& str) {
    std::string lower;
    lower.reserve(str.size());
    for (char c : str) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    
    if (lower == "main") return SessionTarget::Main;
    return SessionTarget::Isolated;
}

}
