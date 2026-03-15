#include "sop.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace zeroclaw {
namespace sop {

// ── SOP directory helpers ────────────────────────────────────────

std::filesystem::path resolve_sops_dir(const std::filesystem::path& workspace_dir,
                                        const std::string& config_dir) {
    if (!config_dir.empty()) {
        return std::filesystem::path(config_dir);
    }
    return workspace_dir / "sops";
}

// ── Markdown step parser ─────────────────────────────────────────

/// Try to parse "N. rest" from a line; returns rest if found
static std::optional<std::string> parse_numbered_item(const std::string& line) {
    auto dot_pos = line.find(". ");
    if (dot_pos == std::string::npos || dot_pos == 0) return std::nullopt;
    for (size_t i = 0; i < dot_pos; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(line[i]))) return std::nullopt;
    }
    return line.substr(dot_pos + 2);
}

/// Extract **title** from beginning of text, returning (title, rest)
static std::optional<std::pair<std::string, std::string>> extract_bold_title(const std::string& text) {
    auto start = text.find("**");
    if (start == std::string::npos) return std::nullopt;
    size_t after_start = start + 2;
    auto end = text.find("**", after_start);
    if (end == std::string::npos) return std::nullopt;
    std::string title = text.substr(after_start, end - after_start);
    size_t rest_start = end + 2;
    std::string rest = text.substr(rest_start);
    // Trim leading em dash, en dash, or hyphen separator
    size_t s = rest.find_first_not_of(" \t");
    if (s != std::string::npos) rest = rest.substr(s);
    for (const char* sep : {"\xe2\x80\x94", "\xe2\x80\x93", "-"}) {
        if (rest.substr(0, strlen(sep)) == sep) {
            rest = rest.substr(strlen(sep));
            break;
        }
    }
    s = rest.find_first_not_of(" \t");
    if (s != std::string::npos) rest = rest.substr(s);
    return std::make_pair(title, rest);
}

/// Matching Rust parse_steps() — parses ## Steps section of SOP.md
std::vector<SopStep> parse_steps(const std::string& md) {
    std::vector<SopStep> steps;

    bool in_steps = false;
    std::optional<uint32_t> current_number;
    std::string current_title;
    std::string current_body;
    std::vector<std::string> current_tools;
    bool current_requires_confirmation = false;

    auto flush_step = [&]() {
        if (!current_number.has_value()) return;
        SopStep step;
        step.number = *current_number;
        step.title = current_title;
        // Trim trailing whitespace from body
        size_t e = current_body.find_last_not_of(" \t\r\n");
        step.body = (e != std::string::npos) ? current_body.substr(0, e + 1) : "";
        step.suggested_tools = current_tools;
        step.requires_confirmation = current_requires_confirmation;
        steps.push_back(std::move(step));
        current_number.reset();
        current_title.clear();
        current_body.clear();
        current_tools.clear();
        current_requires_confirmation = false;
    };

    std::istringstream stream(md);
    std::string line;
    while (std::getline(stream, line)) {
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string trimmed = line;
        // Ltrim
        size_t s = trimmed.find_first_not_of(" \t");
        trimmed = (s != std::string::npos) ? trimmed.substr(s) : "";

        // Detect ## Steps heading
        if (trimmed.size() >= 3 && trimmed.substr(0, 3) == "## ") {
            std::string heading = trimmed.substr(3);
            std::string lower = heading;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower == "steps") {
                in_steps = true;
                continue;
            }
            if (in_steps) {
                flush_step();
                in_steps = false;
            }
            continue;
        }

        if (!in_steps) continue;

        // Check for numbered item
        auto rest_opt = parse_numbered_item(trimmed);
        if (rest_opt.has_value()) {
            flush_step();
            uint32_t step_num = static_cast<uint32_t>(steps.size()) + 1;
            current_number = step_num;

            auto bold = extract_bold_title(*rest_opt);
            if (bold.has_value()) {
                current_title = bold->first;
                current_body = bold->second;
            } else {
                current_title = *rest_opt;
            }
            continue;
        }

        // Sub-bullet inside a step
        if (current_number.has_value() && trimmed.size() >= 2 && trimmed.substr(0, 2) == "- ") {
            std::string bullet = trimmed.substr(2);
            if (bullet.substr(0, 6) == "tools:") {
                std::string tools_str = bullet.substr(6);
                current_tools.clear();
                std::istringstream ts(tools_str);
                std::string tok;
                while (std::getline(ts, tok, ',')) {
                    // trim
                    size_t fs = tok.find_first_not_of(" \t");
                    size_t fe = tok.find_last_not_of(" \t");
                    if (fs != std::string::npos) {
                        current_tools.push_back(tok.substr(fs, fe - fs + 1));
                    }
                }
            } else if (bullet.substr(0, 22) == "requires_confirmation:") {
                std::string val = bullet.substr(22);
                size_t vs = val.find_first_not_of(" \t");
                if (vs != std::string::npos) val = val.substr(vs);
                std::transform(val.begin(), val.end(), val.begin(), ::tolower);
                current_requires_confirmation = (val == "true");
            } else {
                if (!current_body.empty()) current_body += "\n";
                current_body += trimmed;
            }
            continue;
        }

        // Continuation line
        if (current_number.has_value() && !trimmed.empty()) {
            if (!current_body.empty()) current_body += "\n";
            current_body += trimmed;
        }
    }
    flush_step();
    return steps;
}

// ── SOP loading ──────────────────────────────────────────────────

static std::optional<SopPriority> parse_priority(const std::string& s) {
    if (s == "low") return SopPriority::Low;
    if (s == "normal") return SopPriority::Normal;
    if (s == "high") return SopPriority::High;
    if (s == "critical") return SopPriority::Critical;
    return std::nullopt;
}

static std::optional<SopExecutionMode> parse_exec_mode(const std::string& s) {
    if (s == "supervised") return SopExecutionMode::Supervised;
    if (s == "auto") return SopExecutionMode::Auto;
    return std::nullopt;
}

/// Load a single SOP from a directory containing SOP.toml and optionally SOP.md.
/// Mirrors Rust load_sop() — TOML is parsed with a minimal key=value reader since
/// there is no TOML library bundled. Complex nested tables are unsupported.
static std::optional<Sop> load_sop_from_dir(const fs::path& sop_dir,
                                              SopExecutionMode default_mode) {
    fs::path toml_path = sop_dir / "SOP.toml";
    if (!fs::exists(toml_path)) return std::nullopt;

    std::ifstream f(toml_path);
    if (!f) return std::nullopt;

    Sop sop;
    sop.execution_mode = default_mode;
    sop.location = sop_dir;

    std::string line;
    bool in_sop_section = false;
    bool in_triggers_section = false;
    SopTriggerWebhook pending_webhook;
    SopTriggerMqtt pending_mqtt;
    SopTriggerCron pending_cron;
    SopTriggerPeripheral pending_peripheral;
    std::string pending_trigger_type;

    auto flush_trigger = [&]() {
        if (pending_trigger_type == "manual") {
            sop.triggers.emplace_back(SopTriggerManual{});
        } else if (pending_trigger_type == "webhook") {
            sop.triggers.emplace_back(pending_webhook);
            pending_webhook = {};
        } else if (pending_trigger_type == "cron") {
            sop.triggers.emplace_back(pending_cron);
            pending_cron = {};
        } else if (pending_trigger_type == "mqtt") {
            sop.triggers.emplace_back(pending_mqtt);
            pending_mqtt = {};
        } else if (pending_trigger_type == "peripheral") {
            sop.triggers.emplace_back(pending_peripheral);
            pending_peripheral = {};
        }
        pending_trigger_type.clear();
    };

    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = line;
        size_t s = t.find_first_not_of(" \t");
        t = (s != std::string::npos) ? t.substr(s) : "";

        if (t.empty() || t[0] == '#') continue;

        if (t == "[sop]") { in_sop_section = true; in_triggers_section = false; continue; }
        if (t == "[[triggers]]") {
            flush_trigger();
            in_sop_section = false;
            in_triggers_section = true;
            continue;
        }
        if (!t.empty() && t[0] == '[') { in_sop_section = false; in_triggers_section = false; continue; }

        // Parse key = "value"
        auto eq = t.find(" = ");
        if (eq == std::string::npos) { eq = t.find('='); }
        if (eq == std::string::npos) continue;
        std::string key = t.substr(0, eq);
        std::string val = t.substr(eq + (t.find(" = ") != std::string::npos ? 3 : 1));
        // Strip quotes and whitespace
        size_t vs = val.find_first_not_of(" \t\"");
        size_t ve = val.find_last_not_of(" \t\"");
        if (vs == std::string::npos) val = "";
        else val = val.substr(vs, ve - vs + 1);

        if (in_sop_section) {
            if (key == "name") sop.name = val;
            else if (key == "description") sop.description = val;
            else if (key == "version") sop.version = val;
            else if (key == "priority") {
                auto p = parse_priority(val);
                if (p) sop.priority = *p;
            } else if (key == "execution_mode") {
                auto m = parse_exec_mode(val);
                if (m) sop.execution_mode = *m;
            } else if (key == "cooldown_secs") {
                try { sop.cooldown_secs = std::stoull(val); } catch (...) {}
            } else if (key == "max_concurrent") {
                try { sop.max_concurrent = static_cast<uint32_t>(std::stoul(val)); } catch (...) {}
            }
        }
        if (in_triggers_section) {
            if (key == "type") pending_trigger_type = val;
            else if (key == "path") pending_webhook.path = val;
            else if (key == "expression") pending_cron.expression = val;
            else if (key == "topic") pending_mqtt.topic = val;
            else if (key == "condition") {
                if (pending_trigger_type == "mqtt") pending_mqtt.condition = val;
                else if (pending_trigger_type == "peripheral") pending_peripheral.condition = val;
            }
            else if (key == "board") pending_peripheral.board = val;
            else if (key == "signal") pending_peripheral.signal = val;
        }
    }
    flush_trigger();

    if (sop.name.empty()) return std::nullopt;

    // Parse SOP.md steps
    fs::path md_path = sop_dir / "SOP.md";
    if (fs::exists(md_path)) {
        std::ifstream mf(md_path);
        if (mf) {
            std::string md((std::istreambuf_iterator<char>(mf)), std::istreambuf_iterator<char>());
            sop.steps = parse_steps(md);
        }
    }

    return sop;
}

std::vector<Sop> load_sops_from_directory(const fs::path& sops_dir,
                                            SopExecutionMode default_mode) {
    std::vector<Sop> sops;
    if (!fs::exists(sops_dir)) return sops;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(sops_dir, ec)) {
        if (ec) break;
        if (!entry.is_directory()) continue;
        auto sop_opt = load_sop_from_dir(entry.path(), default_mode);
        if (sop_opt.has_value()) {
            sops.push_back(std::move(*sop_opt));
        }
    }

    // Sort by name — matching Rust
    std::sort(sops.begin(), sops.end(), [](const Sop& a, const Sop& b) {
        return a.name < b.name;
    });
    return sops;
}

std::vector<Sop> load_sops(const fs::path& workspace_dir,
                             const std::string& config_dir,
                             SopExecutionMode default_mode) {
    auto dir = resolve_sops_dir(workspace_dir, config_dir);
    return load_sops_from_directory(dir, default_mode);
}

// ── Validation ───────────────────────────────────────────────────

/// Matching Rust validate_sop() — returns list of warnings
std::vector<std::string> validate_sop(const Sop& sop) {
    std::vector<std::string> warnings;

    if (sop.name.empty()) warnings.push_back("SOP name is empty");
    if (sop.description.empty()) warnings.push_back("SOP description is empty");
    if (sop.triggers.empty()) warnings.push_back("SOP has no triggers defined");
    if (sop.steps.empty()) warnings.push_back("SOP has no steps (missing or empty SOP.md)");

    // Check step numbering continuity
    for (size_t i = 0; i < sop.steps.size(); ++i) {
        uint32_t expected = static_cast<uint32_t>(i) + 1;
        if (sop.steps[i].number != expected) {
            warnings.push_back("Step numbering gap: expected " + std::to_string(expected) +
                                ", got " + std::to_string(sop.steps[i].number));
        }
        if (sop.steps[i].title.empty()) {
            warnings.push_back("Step " + std::to_string(sop.steps[i].number) + " has an empty title");
        }
    }

    return warnings;
}

} // namespace sop
} // namespace zeroclaw
