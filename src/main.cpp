#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <map>

#include "CLI11.hpp"

// Core modules
#include "config/config.hpp"
#include "agent/agent.hpp"
#include "agent/agent_module.hpp"
#include "gateway/gateway.hpp"
#include "daemon/daemon.hpp"
// #include "channels/channels.hpp"
#include "providers_module.hpp"
#include <iomanip>

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

    // --- Cron Command ---
    auto* cron_cmd = app.add_subcommand("cron", "Configure and manage scheduled tasks (stubs)");
    auto* cron_list = cron_cmd->add_subcommand("list", "List all scheduled tasks");
    auto* cron_add = cron_cmd->add_subcommand("add", "Add a new scheduled task");

    // --- Channel Command ---
    auto* channel_cmd = app.add_subcommand("channel", "Manage communication channels (stubs)");
    auto* channel_list = channel_cmd->add_subcommand("list", "List all configured channels");
    auto* channel_doctor = channel_cmd->add_subcommand("doctor", "Check channel configuration health");

    // (TODO: Add remaining commands: onboard, service, doctor, estop, models, integrations, skills, migrate, auth, hardware, peripheral, memory, config, completions)

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

    // Load configuration
    config::Config cfg;
    try {
        cfg = config::Config::load_or_init(config_dir);
        cfg.apply_env_overrides();
    } catch (const std::exception& e) {
        std::cerr << "Config error: " << e.what() << "\n";
        return 1;
    }

    try {
        if (agent_cmd->parsed()) {
            std::cout << "Starting ZeroClaw++ Agent loop...\n";
            std::string result = zeroclaw::agent::run(cfg, agent_msg, 
                agent_provider.empty() ? std::nullopt : std::make_optional(agent_provider),
                agent_model.empty() ? std::nullopt : std::make_optional(agent_model),
                agent_temp, agent_peripherals, true);
            if (!result.empty()) {
                std::cout << result << "\n";
            }
        } else if (gateway_cmd->parsed()) {
            int port = gateway_port > 0 ? gateway_port : cfg.gateway.port;
            std::string host = !gateway_host.empty() ? gateway_host : cfg.gateway.host;
            std::cout << "Starting ZeroClaw++ Gateway on " << host << ":" << port << "...\n";
            zeroclaw::gateway::run_gateway(host, port, cfg);
        } else if (daemon_cmd->parsed()) {
            int port = daemon_port > 0 ? daemon_port : cfg.gateway.port;
            std::string host = !daemon_host.empty() ? daemon_host : cfg.gateway.host;
            if (port == 0) {
                std::cout << "Starting ZeroClaw++ Daemon on " << host << " (random port)\n";
            } else {
                std::cout << "Starting ZeroClaw++ Daemon on " << host << ":" << port << "\n";
            }
            auto cfg_ptr = std::make_shared<config::Config>(cfg);
            auto future = zeroclaw::daemon::run(cfg_ptr, host, port);
            future.wait();
        } else if (status_cmd->parsed()) {
            std::cout << "ZeroClaw++ Status\n\n";
            std::cout << "Version:     0.1.0\n";
            std::cout << "Workspace:   " << cfg.workspace_dir.string() << "\n";
            std::cout << "Config:      " << cfg.config_path.string() << "\n\n";
            
            std::cout << "Provider:      " << cfg.default_provider.value_or("openrouter") << "\n";
            std::cout << "Model:         " << cfg.default_model.value_or("(default)") << "\n";
            std::cout << "Observability:  " << cfg.observability.backend << "\n";
            std::cout << "Trace storage:  " << cfg.observability.runtime_trace_mode << " (" << cfg.observability.runtime_trace_path << ")\n";
            std::cout << "Autonomy:      Standard\n";
            std::cout << "Runtime:       " << cfg.runtime.kind << "\n";
            std::cout << "Heartbeat:      ";
            if (cfg.heartbeat.enabled) {
                std::cout << "every " << cfg.heartbeat.interval_minutes << "min\n";
            } else {
                std::cout << "disabled\n";
            }
            std::cout << "Memory:         " << cfg.memory.backend << " (auto-save: " << (cfg.memory.auto_save ? "on" : "off") << ")\n\n";
            
            std::cout << "Security:\n";
            std::cout << "  Workspace only:    " << (cfg.autonomy.workspace_only ? "true" : "false") << "\n";
            std::cout << "  Max actions/hour:  " << cfg.autonomy.max_actions_per_hour << "\n";
            std::cout << "  OTP enabled:       " << (cfg.security.otp.enabled ? "true" : "false") << "\n";
            std::cout << "  E-stop enabled:    " << (cfg.security.estop.enabled ? "true" : "false") << "\n\n";
            
            std::cout << "Peripherals:\n";
            std::cout << "  Enabled:   " << (cfg.peripherals.enabled ? "yes" : "no") << "\n";
            std::cout << "  Boards:    " << cfg.peripherals.boards.size() << "\n";
        } else if (providers_cmd->parsed()) {
            auto providers = zeroclaw::providers::list_providers();
            std::cout << "Supported providers (" << providers.size() << " total):\n\n";
            std::cout << "  ID (use in config)  DESCRIPTION\n";
            std::cout << "  ------------------- -----------\n";
            for (const auto& p : providers) {
                std::cout << "  " << std::left << std::setw(19) << p.name;
                if (!p.aliases.empty()) {
                    std::cout << " (aliases: ";
                    for (size_t i = 0; i < p.aliases.size(); ++i) {
                        std::cout << p.aliases[i];
                        if (i + 1 < p.aliases.size()) std::cout << ", ";
                    }
                    std::cout << ")";
                }
                std::cout << "\n";
            }
            std::cout << "\n  custom:<URL>   Any OpenAI-compatible endpoint\n";
            std::cout << "  anthropic-custom:<URL>  Any Anthropic-compatible endpoint\n";
        } else if (cron_cmd->parsed()) {
            if (cron_list->parsed()) {
                std::cout << "Listing scheduled tasks... (stub)\n";
            } else if (cron_add->parsed()) {
                std::cout << "Adding new scheduled task... (stub)\n";
            } else {
                std::cout << "Cron management (stub). Use --help for subcommands.\n";
            }
        } else if (channel_cmd->parsed()) {
            if (channel_list->parsed()) {
                std::cout << "Listing configured channels... (stub)\n";
            } else if (channel_doctor->parsed()) {
                std::cout << "Running channel doctor... (stub)\n";
            } else {
                std::cout << "Channel management (stub). Use --help for subcommands.\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
