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
#include "cron/cron.hpp"
#include "channels_module.hpp"
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
    auto* cron_cmd = app.add_subcommand("cron", "Configure and manage scheduled tasks");
    cron_cmd->require_subcommand(1);

    // cron list
    auto* cron_list = cron_cmd->add_subcommand("list", "List all scheduled tasks");

    // cron add <expression> <command> [--tz TZ]
    auto* cron_add = cron_cmd->add_subcommand("add", "Add a new recurring scheduled task");
    std::string cron_add_expr, cron_add_command;
    std::string cron_add_tz;
    cron_add->add_option("expression", cron_add_expr, "Cron expression (e.g. '0 9 * * 1-5')")->required();
    cron_add->add_option("command", cron_add_command, "Command to run")->required();
    cron_add->add_option("--tz", cron_add_tz, "Optional IANA timezone (e.g. America/Los_Angeles)");

    // cron add-at <at> <command>
    auto* cron_add_at = cron_cmd->add_subcommand("add-at", "Add a one-shot scheduled task at an RFC3339 timestamp");
    std::string cron_at_ts, cron_at_command;
    cron_add_at->add_option("at", cron_at_ts, "One-shot timestamp in RFC3339 format")->required();
    cron_add_at->add_option("command", cron_at_command, "Command to run")->required();

    // cron add-every <every_ms> <command>
    auto* cron_add_every = cron_cmd->add_subcommand("add-every", "Add a fixed-interval scheduled task");
    uint64_t cron_every_ms = 0;
    std::string cron_every_command;
    cron_add_every->add_option("every_ms", cron_every_ms, "Interval in milliseconds")->required();
    cron_add_every->add_option("command", cron_every_command, "Command to run")->required();

    // cron once <delay> <command>
    auto* cron_once = cron_cmd->add_subcommand("once", "Add a one-shot delayed task (e.g. '30m', '2h', '1d')");
    std::string cron_once_delay, cron_once_command;
    cron_once->add_option("delay", cron_once_delay, "Delay duration")->required();
    cron_once->add_option("command", cron_once_command, "Command to run")->required();

    // cron remove <id>
    auto* cron_remove = cron_cmd->add_subcommand("remove", "Remove a scheduled task");
    std::string cron_remove_id;
    cron_remove->add_option("id", cron_remove_id, "Task ID")->required();

    // cron update <id> [--expression EXPR] [--tz TZ] [--command CMD] [--name NAME]
    auto* cron_update = cron_cmd->add_subcommand("update", "Update a scheduled task");
    std::string cron_update_id;
    std::string cron_update_expr, cron_update_tz, cron_update_cmd, cron_update_name;
    cron_update->add_option("id", cron_update_id, "Task ID")->required();
    cron_update->add_option("--expression", cron_update_expr, "New cron expression");
    cron_update->add_option("--tz", cron_update_tz, "New IANA timezone");
    cron_update->add_option("--command", cron_update_cmd, "New command to run");
    cron_update->add_option("--name", cron_update_name, "New job name");

    // cron pause <id>
    auto* cron_pause = cron_cmd->add_subcommand("pause", "Pause a scheduled task");
    std::string cron_pause_id;
    cron_pause->add_option("id", cron_pause_id, "Task ID")->required();

    // cron resume <id>
    auto* cron_resume = cron_cmd->add_subcommand("resume", "Resume a paused task");
    std::string cron_resume_id;
    cron_resume->add_option("id", cron_resume_id, "Task ID")->required();

    // --- Channel Command ---
    auto* channel_cmd = app.add_subcommand("channel", "Manage communication channels");
    channel_cmd->require_subcommand(1);

    // channel list
    auto* channel_list = channel_cmd->add_subcommand("list", "List all configured channels");

    // channel doctor
    auto* channel_doctor = channel_cmd->add_subcommand("doctor", "Check channel configuration health");

    // channel start
    auto* channel_start = channel_cmd->add_subcommand("start", "Start all configured channels");

    // channel add <type> <config_json>
    auto* channel_add = channel_cmd->add_subcommand("add", "Add a new channel configuration");
    std::string channel_add_type, channel_add_config;
    channel_add->add_option("channel_type", channel_add_type, "Channel type (telegram, discord, slack, etc.)")->required();
    channel_add->add_option("config", channel_add_config, "Configuration as JSON")->required();

    // channel remove <name>
    auto* channel_remove = channel_cmd->add_subcommand("remove", "Remove a channel configuration");
    std::string channel_remove_name;
    channel_remove->add_option("name", channel_remove_name, "Channel name to remove")->required();

    // channel send <message> --channel-id <id> --recipient <id>
    auto* channel_send = channel_cmd->add_subcommand("send", "Send a message to a configured channel");
    std::string channel_send_msg, channel_send_id, channel_send_recipient;
    channel_send->add_option("message", channel_send_msg, "Message text to send")->required();
    channel_send->add_option("--channel-id", channel_send_id, "Channel config name")->required();
    channel_send->add_option("--recipient", channel_send_recipient, "Recipient identifier")->required();

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

            std::cout << "Channels:\n";
            std::cout << "  CLI:      always\n";
            auto channels = zeroclaw::channels::list_configured_channels(cfg);
            for (const auto& [name, configured] : channels) {
                std::cout << "  " << std::left << std::setw(9) << name << " "
                          << (configured ? "configured" : "not configured") << "\n";
            }
            std::cout << "\n";

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
            // Dispatch to cron::handle_command
            if (cron_list->parsed()) {
                cron::handle_command(cfg, "list");
            } else if (cron_add->parsed()) {
                std::optional<std::string> tz_opt = cron_add_tz.empty() ? std::nullopt : std::make_optional(cron_add_tz);
                cron::handle_command(cfg, "add", cron_add_expr, tz_opt, cron_add_command);
            } else if (cron_add_at->parsed()) {
                cron::handle_command(cfg, "add-at", "", std::nullopt, cron_at_command, cron_at_ts);
            } else if (cron_add_every->parsed()) {
                cron::handle_command(cfg, "add-every", "", std::nullopt, cron_every_command, "", cron_every_ms);
            } else if (cron_once->parsed()) {
                cron::handle_command(cfg, "once", "", std::nullopt, cron_once_command, "", 0, cron_once_delay);
            } else if (cron_remove->parsed()) {
                cron::handle_command(cfg, "remove", "", std::nullopt, "", "", 0, "", cron_remove_id);
            } else if (cron_update->parsed()) {
                std::optional<std::string> expr = cron_update_expr.empty() ? std::nullopt : std::make_optional(cron_update_expr);
                std::optional<std::string> tz = cron_update_tz.empty() ? std::nullopt : std::make_optional(cron_update_tz);
                std::optional<std::string> cmd = cron_update_cmd.empty() ? std::nullopt : std::make_optional(cron_update_cmd);
                std::optional<std::string> name = cron_update_name.empty() ? std::nullopt : std::make_optional(cron_update_name);
                cron::handle_command(cfg, "update", "", std::nullopt, "", "", 0, "", cron_update_id, expr, tz, cmd, name);
            } else if (cron_pause->parsed()) {
                cron::handle_command(cfg, "pause", "", std::nullopt, "", "", 0, "", cron_pause_id);
            } else if (cron_resume->parsed()) {
                cron::handle_command(cfg, "resume", "", std::nullopt, "", "", 0, "", cron_resume_id);
            }
        } else if (channel_cmd->parsed()) {
            // Dispatch to channels::handle_command
            if (channel_list->parsed()) {
                channels::handle_command(cfg, "list");
            } else if (channel_doctor->parsed()) {
                channels::handle_command(cfg, "doctor");
            } else if (channel_start->parsed()) {
                channels::handle_command(cfg, "start");
            } else if (channel_add->parsed()) {
                channels::handle_command(cfg, "add", channel_add_type, channel_add_config);
            } else if (channel_remove->parsed()) {
                channels::handle_command(cfg, "remove", channel_remove_name);
            } else if (channel_send->parsed()) {
                channels::handle_command(cfg, "send", channel_send_msg, channel_send_id, channel_send_recipient);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
