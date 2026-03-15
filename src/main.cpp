#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <map>

#include "CLI11.hpp"

// Core modules
#include "config/config.hpp"
#include "agent/agent.hpp"
// #include "gateway/gateway.hpp"
// #include "daemon/daemon.hpp"
// #include "channels/channels.hpp"

using namespace zeroclaw;

int main(int argc, char* argv[]) {
    CLI::App app{"ZeroClaw++ - Zero overhead. Zero compromise. 100% C++.\nThe fastest, smallest AI assistant."};
    app.require_subcommand(1); // Require exactly 1 subcommand

    // Global Options
    std::string config_dir;
    app.add_option("--config-dir", config_dir, "Path to configuration directory")->expected(1);

    // --- Agent Command ---
    auto* agent_cmd = app.add_subcommand("agent", "Start the AI agent loop\n\nLaunches an interactive chat session with the configured AI provider. Use --message for single-shot queries without entering interactive mode.");
    
    std::string agent_msg;
    agent_cmd->add_option("-m,--message", agent_msg, "Single message mode (don't enter interactive mode)");
    
    std::string agent_provider;
    agent_cmd->add_option("-p,--provider", agent_provider, "Provider to use (openrouter, anthropic, openai, openai-codex)");
    
    std::string agent_model;
    agent_cmd->add_option("--model", agent_model, "Model to use");
    
    double agent_temp = 0.7;
    agent_cmd->add_option("-t,--temperature", agent_temp, "Temperature (0.0 - 2.0)")
        ->check(CLI::Range(0.0, 2.0));
    
    std::vector<std::string> agent_peripherals;
    agent_cmd->add_option("--peripheral", agent_peripherals, "Attach a peripheral (board:path, e.g. nucleo-f401re:/dev/ttyACM0)");

    // --- Gateway Command ---
    auto* gateway_cmd = app.add_subcommand("gateway", "Start the gateway server (webhooks, websockets)");
    int gateway_port = -1;
    gateway_cmd->add_option("-p,--port", gateway_port, "Port to listen on (use 0 for random available port); defaults to config gateway.port");
    std::string gateway_host;
    gateway_cmd->add_option("--host", gateway_host, "Host to bind to; defaults to config gateway.host");

    // --- Daemon Command ---
    auto* daemon_cmd = app.add_subcommand("daemon", "Start the long-running autonomous daemon");
    int daemon_port = -1;
    daemon_cmd->add_option("-p,--port", daemon_port, "Port to listen on (use 0 for random available port); defaults to config gateway.port");
    std::string daemon_host;
    daemon_cmd->add_option("--host", daemon_host, "Host to bind to; defaults to config gateway.host");

    // --- Status Command ---
    auto* status_cmd = app.add_subcommand("status", "Show system status (full details)");

    // --- Providers Command ---
    auto* providers_cmd = app.add_subcommand("providers", "List supported AI providers");

    // (TODO: Add remaining 12 commands: onboard, service, doctor, estop, cron, models, channel, integrations, skills, migrate, auth, hardware, peripheral, memory, config, completions)

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    if (!config_dir.empty()) {
        // Equivalent to std::env::set_var("ZEROCLAW_CONFIG_DIR", config_dir) in Rust
        // Since C++ doesn't have a standard setenv on Windows that propagates to all code trivially,
        // we might just pass it to Config::load_or_init.
    }

    // TODO: Load configuration
    // config::Config cfg;
    // try {
    //     cfg = config::Config::load_or_init(config_dir);
    //     cfg.apply_env_overrides();
    // } catch (const std::exception& e) {
    //     std::cerr << "Config error: " << e.what() << "\n";
    //     return 1;
    // }

    try {
        if (agent_cmd->parsed()) {
            std::cout << "Starting Agent loop (stub)...\n";
            // zeroclaw::agent::run(cfg, agent_msg, agent_provider, agent_model, agent_temp, agent_peripherals, true);
        } else if (gateway_cmd->parsed()) {
            std::cout << "Starting Gateway (stub)...\n";
        } else if (daemon_cmd->parsed()) {
            std::cout << "Starting Daemon (stub)...\n";
        } else if (status_cmd->parsed()) {
            std::cout << "ZeroClaw++ Status Check (stub)...\n";
        } else if (providers_cmd->parsed()) {
            std::cout << "Supported Providers (stub)...\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
