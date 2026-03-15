#include "store.hpp"
#include "schedule.hpp"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <random>
#include "sqlite-3.51.2/sqlite3.h"

namespace zeroclaw::cron {

namespace {

// ── UUID generation ─────────────────────────────────────────────
std::string generate_uuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; ++i) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; ++i) ss << dis(gen);
    ss << "-4";
    for (int i = 0; i < 3; ++i) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; ++i) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; ++i) ss << dis(gen);
    return ss.str();
}

// ── RFC3339 formatting ──────────────────────────────────────────
std::string format_rfc3339(TimePoint tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

TimePoint parse_rfc3339(const std::string& str) {
    std::tm tm = {};
    std::istringstream iss(str);
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
#ifdef _WIN32
    auto tp = std::chrono::system_clock::from_time_t(_mkgmtime(&tm));
#else
    auto tp = std::chrono::system_clock::from_time_t(timegm(&tm));
#endif
    size_t dot_pos = str.find('.');
    if (dot_pos != std::string::npos) {
        std::string ms_str = str.substr(dot_pos + 1);
        ms_str = ms_str.substr(0, ms_str.find('Z'));
        while (ms_str.size() < 3) ms_str += '0';
        if (ms_str.size() > 3) ms_str = ms_str.substr(0, 3);
        int ms = std::stoi(ms_str);
        tp += std::chrono::milliseconds(ms);
    }
    return tp;
}

// ── JSON serialization (minimal, no external lib dependency) ────
std::string escape_json_string(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:   result += c;      break;
        }
    }
    return result;
}

std::string schedule_to_json(const Schedule& schedule) {
    if (auto* cron = std::get_if<ScheduleCron>(&schedule)) {
        std::string json = R"({"kind":"cron","expr":")" + escape_json_string(cron->expr) + "\"";
        if (cron->tz) {
            json += R"(,"tz":")" + escape_json_string(*cron->tz) + "\"";
        }
        json += "}";
        return json;
    }
    if (auto* at = std::get_if<ScheduleAt>(&schedule)) {
        return R"({"kind":"at","at":")" + format_rfc3339(at->at) + "\"}";
    }
    if (auto* every = std::get_if<ScheduleEvery>(&schedule)) {
        return R"({"kind":"every","every_ms":)" + std::to_string(every->every_ms.count()) + "}";
    }
    return "{}";
}

std::string delivery_to_json(const DeliveryConfig& delivery) {
    std::string json = R"({"mode":")" + escape_json_string(delivery.mode) + "\"";
    if (delivery.channel) {
        json += R"(,"channel":")" + escape_json_string(*delivery.channel) + "\"";
    }
    if (delivery.to) {
        json += R"(,"to":")" + escape_json_string(*delivery.to) + "\"";
    }
    json += R"(,"best_effort":)" + std::string(delivery.best_effort ? "true" : "false") + "}";
    return json;
}

// ── Minimal JSON parsing helpers ────────────────────────────────
// Extract a JSON string value for a given key from a flat JSON object
std::optional<std::string> json_string_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return std::nullopt;
    pos += search.size();
    std::string value;
    for (size_t i = pos; i < json.size(); ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            char next = json[i + 1];
            switch (next) {
                case '"':  value += '"';  break;
                case '\\': value += '\\'; break;
                case 'n':  value += '\n'; break;
                case 'r':  value += '\r'; break;
                case 't':  value += '\t'; break;
                default:   value += next; break;
            }
            ++i;
        } else if (json[i] == '"') {
            break;
        } else {
            value += json[i];
        }
    }
    return value;
}

std::optional<int64_t> json_int_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) return std::nullopt;
    pos += search.size();
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    if (pos >= json.size()) return std::nullopt;
    // Check for string-quoted number
    if (json[pos] == '"') return std::nullopt;
    std::string num_str;
    while (pos < json.size() && (std::isdigit(static_cast<unsigned char>(json[pos])) || json[pos] == '-')) {
        num_str += json[pos++];
    }
    if (num_str.empty()) return std::nullopt;
    return std::stoll(num_str);
}

std::optional<bool> json_bool_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) return std::nullopt;
    pos += search.size();
    while (pos < json.size() && json[pos] == ' ') ++pos;
    if (json.substr(pos, 4) == "true") return true;
    if (json.substr(pos, 5) == "false") return false;
    return std::nullopt;
}

Schedule decode_schedule(const std::string& schedule_raw, const std::string& expression) {
    if (!schedule_raw.empty()) {
        auto kind = json_string_value(schedule_raw, "kind");
        if (kind) {
            if (*kind == "cron") {
                auto expr = json_string_value(schedule_raw, "expr");
                auto tz = json_string_value(schedule_raw, "tz");
                return ScheduleCron{expr.value_or(expression), tz};
            }
            if (*kind == "at") {
                auto at_str = json_string_value(schedule_raw, "at");
                if (at_str) {
                    return ScheduleAt{parse_rfc3339(*at_str)};
                }
            }
            if (*kind == "every") {
                auto ms = json_int_value(schedule_raw, "every_ms");
                if (ms) {
                    return ScheduleEvery{std::chrono::milliseconds(*ms)};
                }
            }
        }
    }

    if (expression.empty()) {
        throw std::runtime_error("Missing schedule and legacy expression for cron job");
    }
    return ScheduleCron{expression, std::nullopt};
}

DeliveryConfig decode_delivery(const std::string& delivery_raw) {
    DeliveryConfig delivery;
    if (delivery_raw.empty()) return delivery;

    auto mode = json_string_value(delivery_raw, "mode");
    if (mode) delivery.mode = *mode;
    delivery.channel = json_string_value(delivery_raw, "channel");
    delivery.to = json_string_value(delivery_raw, "to");
    auto be = json_bool_value(delivery_raw, "best_effort");
    if (be) delivery.best_effort = *be;
    return delivery;
}

// ── SQLite helpers ──────────────────────────────────────────────

struct SqliteDeleter {
    void operator()(sqlite3* db) { sqlite3_close(db); }
};
using SqlitePtr = std::unique_ptr<sqlite3, SqliteDeleter>;

std::string get_text(sqlite3_stmt* stmt, int col) {
    auto* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

std::optional<std::string> get_optional_text(sqlite3_stmt* stmt, int col) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) return std::nullopt;
    auto* text = sqlite3_column_text(stmt, col);
    return text ? std::optional<std::string>(reinterpret_cast<const char*>(text)) : std::nullopt;
}

CronJob map_row(sqlite3_stmt* stmt) {
    CronJob job;
    job.id = get_text(stmt, 0);
    job.expression = get_text(stmt, 1);
    job.command = get_text(stmt, 2);

    std::string schedule_raw = get_text(stmt, 3);
    job.schedule = decode_schedule(schedule_raw, job.expression);

    std::string jt = get_text(stmt, 4);
    job.job_type = job_type_from_string(jt.empty() ? "shell" : jt);

    job.prompt = get_optional_text(stmt, 5);
    job.name = get_optional_text(stmt, 6);

    std::string st = get_text(stmt, 7);
    job.session_target = session_target_from_string(st.empty() ? "isolated" : st);

    job.model = get_optional_text(stmt, 8);
    job.enabled = sqlite3_column_int64(stmt, 9) != 0;

    std::string delivery_raw = get_text(stmt, 10);
    job.delivery = decode_delivery(delivery_raw);

    job.delete_after_run = sqlite3_column_int64(stmt, 11) != 0;

    std::string created = get_text(stmt, 12);
    if (!created.empty()) job.created_at = parse_rfc3339(created);

    std::string next = get_text(stmt, 13);
    if (!next.empty()) job.next_run = parse_rfc3339(next);

    auto last_run_str = get_optional_text(stmt, 14);
    if (last_run_str && !last_run_str->empty()) {
        job.last_run = parse_rfc3339(*last_run_str);
    }

    job.last_status = get_optional_text(stmt, 15);
    job.last_output = get_optional_text(stmt, 16);
    return job;
}

void add_column_if_missing(sqlite3* db, const char* name, const char* sql_type) {
    // Check if column exists
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, "PRAGMA table_info(cron_jobs)", -1, &stmt, nullptr) == SQLITE_OK) {
        bool found = false;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string col = get_text(stmt, 1);
            if (col == name) { found = true; break; }
        }
        sqlite3_finalize(stmt);
        if (found) return;
    }

    std::string sql = std::string("ALTER TABLE cron_jobs ADD COLUMN ") + name + " " + sql_type;
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK && err) {
        std::string msg(err);
        sqlite3_free(err);
        // Tolerate "duplicate column name" (concurrent migration)
        if (msg.find("duplicate column name") == std::string::npos) {
            throw std::runtime_error("Failed to add cron_jobs." + std::string(name) + ": " + msg);
        }
    } else {
        sqlite3_free(err);
    }
}

sqlite3* open_connection(const std::string& workspace_dir) {
    namespace fs = std::filesystem;
    fs::path db_dir = fs::path(workspace_dir) / "cron";
    fs::create_directories(db_dir);
    fs::path db_path = db_dir / "jobs.db";

    sqlite3* db = nullptr;
    int rc = sqlite3_open(db_path.string().c_str(), &db);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close(db);
        throw std::runtime_error("Failed to open cron DB: " + db_path.string());
    }

    // Initialize schema
    const char* schema = R"(
        PRAGMA foreign_keys = ON;
        CREATE TABLE IF NOT EXISTS cron_jobs (
            id               TEXT PRIMARY KEY,
            expression       TEXT NOT NULL,
            command          TEXT NOT NULL,
            schedule         TEXT,
            job_type         TEXT NOT NULL DEFAULT 'shell',
            prompt           TEXT,
            name             TEXT,
            session_target   TEXT NOT NULL DEFAULT 'isolated',
            model            TEXT,
            enabled          INTEGER NOT NULL DEFAULT 1,
            delivery         TEXT,
            delete_after_run INTEGER NOT NULL DEFAULT 0,
            created_at       TEXT NOT NULL,
            next_run         TEXT NOT NULL,
            last_run         TEXT,
            last_status      TEXT,
            last_output      TEXT
        );
        CREATE INDEX IF NOT EXISTS idx_cron_jobs_next_run ON cron_jobs(next_run);

        CREATE TABLE IF NOT EXISTS cron_runs (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            job_id      TEXT NOT NULL,
            started_at  TEXT NOT NULL,
            finished_at TEXT NOT NULL,
            status      TEXT NOT NULL,
            output      TEXT,
            duration_ms INTEGER,
            FOREIGN KEY (job_id) REFERENCES cron_jobs(id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_cron_runs_job_id ON cron_runs(job_id);
        CREATE INDEX IF NOT EXISTS idx_cron_runs_started_at ON cron_runs(started_at);
        CREATE INDEX IF NOT EXISTS idx_cron_runs_job_started ON cron_runs(job_id, started_at);
    )";

    char* err = nullptr;
    rc = sqlite3_exec(db, schema, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "schema init failed";
        sqlite3_free(err);
        sqlite3_close(db);
        throw std::runtime_error("Failed to initialize cron schema: " + msg);
    }

    // Migration: add columns if missing
    add_column_if_missing(db, "schedule", "TEXT");
    add_column_if_missing(db, "job_type", "TEXT NOT NULL DEFAULT 'shell'");
    add_column_if_missing(db, "prompt", "TEXT");
    add_column_if_missing(db, "name", "TEXT");
    add_column_if_missing(db, "session_target", "TEXT NOT NULL DEFAULT 'isolated'");
    add_column_if_missing(db, "model", "TEXT");
    add_column_if_missing(db, "enabled", "INTEGER NOT NULL DEFAULT 1");
    add_column_if_missing(db, "delivery", "TEXT");
    add_column_if_missing(db, "delete_after_run", "INTEGER NOT NULL DEFAULT 0");

    return db;
}

} // anonymous namespace

// ── Public API ──────────────────────────────────────────────────

std::string truncate_cron_output(const std::string& output) {
    if (output.size() <= MAX_CRON_OUTPUT_BYTES) {
        return output;
    }

    if (MAX_CRON_OUTPUT_BYTES <= std::strlen(TRUNCATED_OUTPUT_MARKER)) {
        return TRUNCATED_OUTPUT_MARKER;
    }

    size_t cutoff = MAX_CRON_OUTPUT_BYTES - std::strlen(TRUNCATED_OUTPUT_MARKER);
    std::string truncated = output.substr(0, cutoff);
    truncated += TRUNCATED_OUTPUT_MARKER;
    return truncated;
}

CronJob add_job(const std::string& workspace_dir, int /*max_run_history*/, const std::string& expression, const std::string& command) {
    Schedule schedule = ScheduleCron{expression, std::nullopt};
    return add_shell_job(workspace_dir, 50, 100, std::nullopt, schedule, command);
}

CronJob add_shell_job(
    const std::string& workspace_dir,
    int /*max_run_history*/,
    int /*max_tasks*/,
    const std::optional<std::string>& name,
    const Schedule& schedule,
    const std::string& command
) {
    auto now = std::chrono::system_clock::now();
    validate_schedule(schedule, now);
    auto next_run = next_run_for_schedule(schedule, now);
    std::string id = generate_uuid();
    auto expr = schedule_cron_expression(schedule);
    std::string expression = expr.value_or("");
    std::string schedule_json = schedule_to_json(schedule);
    bool delete_after_run = std::holds_alternative<ScheduleAt>(schedule);
    std::string delivery_json = delivery_to_json(DeliveryConfig{});

    SqlitePtr db(open_connection(workspace_dir));

    const char* sql = R"(
        INSERT INTO cron_jobs (
            id, expression, command, schedule, job_type, prompt, name, session_target, model,
            enabled, delivery, delete_after_run, created_at, next_run
        ) VALUES (?1, ?2, ?3, ?4, 'shell', NULL, ?5, 'isolated', NULL, 1, ?6, ?7, ?8, ?9)
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare insert: " + std::string(sqlite3_errmsg(db.get())));
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, expression.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, command.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, schedule_json.c_str(), -1, SQLITE_TRANSIENT);
    if (name) {
        sqlite3_bind_text(stmt, 5, name->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 5);
    }
    sqlite3_bind_text(stmt, 6, delivery_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, delete_after_run ? 1 : 0);
    std::string now_str = format_rfc3339(now);
    sqlite3_bind_text(stmt, 8, now_str.c_str(), -1, SQLITE_TRANSIENT);
    std::string next_str = format_rfc3339(next_run);
    sqlite3_bind_text(stmt, 9, next_str.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to insert cron shell job: " + std::string(sqlite3_errmsg(db.get())));
    }

    return get_job(workspace_dir, id);
}

CronJob add_agent_job(
    const std::string& workspace_dir,
    int /*max_run_history*/,
    int /*max_tasks*/,
    const std::optional<std::string>& name,
    const Schedule& schedule,
    const std::string& prompt,
    SessionTarget session_target,
    const std::optional<std::string>& model,
    const std::optional<DeliveryConfig>& delivery,
    bool delete_after_run
) {
    auto now = std::chrono::system_clock::now();
    validate_schedule(schedule, now);
    auto next_run = next_run_for_schedule(schedule, now);
    std::string id = generate_uuid();
    auto expr = schedule_cron_expression(schedule);
    std::string expression = expr.value_or("");
    std::string schedule_json = schedule_to_json(schedule);
    DeliveryConfig del = delivery.value_or(DeliveryConfig{});
    std::string delivery_json = delivery_to_json(del);

    SqlitePtr db(open_connection(workspace_dir));

    const char* sql = R"(
        INSERT INTO cron_jobs (
            id, expression, command, schedule, job_type, prompt, name, session_target, model,
            enabled, delivery, delete_after_run, created_at, next_run
        ) VALUES (?1, ?2, '', ?3, 'agent', ?4, ?5, ?6, ?7, 1, ?8, ?9, ?10, ?11)
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare insert: " + std::string(sqlite3_errmsg(db.get())));
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, expression.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, schedule_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, prompt.c_str(), -1, SQLITE_TRANSIENT);
    if (name) {
        sqlite3_bind_text(stmt, 5, name->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 5);
    }
    std::string st_str = session_target_to_string(session_target);
    sqlite3_bind_text(stmt, 6, st_str.c_str(), -1, SQLITE_TRANSIENT);
    if (model) {
        sqlite3_bind_text(stmt, 7, model->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 7);
    }
    sqlite3_bind_text(stmt, 8, delivery_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 9, delete_after_run ? 1 : 0);
    std::string now_str = format_rfc3339(now);
    sqlite3_bind_text(stmt, 10, now_str.c_str(), -1, SQLITE_TRANSIENT);
    std::string next_str = format_rfc3339(next_run);
    sqlite3_bind_text(stmt, 11, next_str.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to insert cron agent job: " + std::string(sqlite3_errmsg(db.get())));
    }

    return get_job(workspace_dir, id);
}

std::vector<CronJob> list_jobs(const std::string& workspace_dir) {
    SqlitePtr db(open_connection(workspace_dir));
    std::vector<CronJob> jobs;

    const char* sql = R"(
        SELECT id, expression, command, schedule, job_type, prompt, name, session_target, model,
               enabled, delivery, delete_after_run, created_at, next_run, last_run, last_status, last_output
        FROM cron_jobs ORDER BY next_run ASC
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare list: " + std::string(sqlite3_errmsg(db.get())));
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        try {
            jobs.push_back(map_row(stmt));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Warning: Skipping cron job with unparseable row data: %s\n", e.what());
        }
    }
    sqlite3_finalize(stmt);
    return jobs;
}

CronJob get_job(const std::string& workspace_dir, const std::string& job_id) {
    SqlitePtr db(open_connection(workspace_dir));

    const char* sql = R"(
        SELECT id, expression, command, schedule, job_type, prompt, name, session_target, model,
               enabled, delivery, delete_after_run, created_at, next_run, last_run, last_status, last_output
        FROM cron_jobs WHERE id = ?1
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare get: " + std::string(sqlite3_errmsg(db.get())));
    }

    sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto job = map_row(stmt);
        sqlite3_finalize(stmt);
        return job;
    }
    sqlite3_finalize(stmt);
    throw std::runtime_error("Cron job '" + job_id + "' not found");
}

void remove_job(const std::string& workspace_dir, const std::string& id) {
    SqlitePtr db(open_connection(workspace_dir));

    const char* sql = "DELETE FROM cron_jobs WHERE id = ?1";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare delete: " + std::string(sqlite3_errmsg(db.get())));
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    int changes = sqlite3_changes(db.get());
    sqlite3_finalize(stmt);

    if (changes == 0) {
        throw std::runtime_error("Cron job '" + id + "' not found");
    }
}

std::vector<CronJob> due_jobs(const std::string& workspace_dir, int max_tasks, TimePoint now) {
    SqlitePtr db(open_connection(workspace_dir));
    std::vector<CronJob> jobs;
    int limit = std::max(max_tasks, 1);

    const char* sql = R"(
        SELECT id, expression, command, schedule, job_type, prompt, name, session_target, model,
               enabled, delivery, delete_after_run, created_at, next_run, last_run, last_status, last_output
        FROM cron_jobs
        WHERE enabled = 1 AND next_run <= ?1
        ORDER BY next_run ASC
        LIMIT ?2
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare due_jobs: " + std::string(sqlite3_errmsg(db.get())));
    }

    std::string now_str = format_rfc3339(now);
    sqlite3_bind_text(stmt, 1, now_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        try {
            jobs.push_back(map_row(stmt));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Warning: Skipping cron job with unparseable row data: %s\n", e.what());
        }
    }
    sqlite3_finalize(stmt);
    return jobs;
}

CronJob update_job(const std::string& workspace_dir, int /*max_tasks*/, const std::string& job_id, const CronJobPatch& patch) {
    auto job = get_job(workspace_dir, job_id);
    bool schedule_changed = false;

    if (patch.schedule) {
        auto now = std::chrono::system_clock::now();
        validate_schedule(*patch.schedule, now);
        job.schedule = *patch.schedule;
        job.expression = schedule_cron_expression(job.schedule).value_or("");
        schedule_changed = true;
    }
    if (patch.command) job.command = *patch.command;
    if (patch.prompt) job.prompt = *patch.prompt;
    if (patch.name) job.name = *patch.name;
    if (patch.enabled) job.enabled = *patch.enabled;
    if (patch.delivery) job.delivery = *patch.delivery;
    if (patch.model) job.model = *patch.model;
    if (patch.session_target) job.session_target = *patch.session_target;
    if (patch.delete_after_run) job.delete_after_run = *patch.delete_after_run;

    if (schedule_changed) {
        auto now = std::chrono::system_clock::now();
        job.next_run = next_run_for_schedule(job.schedule, now);
    }

    SqlitePtr db(open_connection(workspace_dir));

    const char* sql = R"(
        UPDATE cron_jobs
        SET expression = ?1, command = ?2, schedule = ?3, job_type = ?4, prompt = ?5, name = ?6,
            session_target = ?7, model = ?8, enabled = ?9, delivery = ?10, delete_after_run = ?11,
            next_run = ?12
        WHERE id = ?13
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare update: " + std::string(sqlite3_errmsg(db.get())));
    }

    sqlite3_bind_text(stmt, 1, job.expression.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, job.command.c_str(), -1, SQLITE_TRANSIENT);
    std::string schedule_json = schedule_to_json(job.schedule);
    sqlite3_bind_text(stmt, 3, schedule_json.c_str(), -1, SQLITE_TRANSIENT);
    std::string jt = job_type_to_string(job.job_type);
    sqlite3_bind_text(stmt, 4, jt.c_str(), -1, SQLITE_TRANSIENT);
    if (job.prompt) {
        sqlite3_bind_text(stmt, 5, job.prompt->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 5);
    }
    if (job.name) {
        sqlite3_bind_text(stmt, 6, job.name->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 6);
    }
    std::string st = session_target_to_string(job.session_target);
    sqlite3_bind_text(stmt, 7, st.c_str(), -1, SQLITE_TRANSIENT);
    if (job.model) {
        sqlite3_bind_text(stmt, 8, job.model->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 8);
    }
    sqlite3_bind_int(stmt, 9, job.enabled ? 1 : 0);
    std::string del_json = delivery_to_json(job.delivery);
    sqlite3_bind_text(stmt, 10, del_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 11, job.delete_after_run ? 1 : 0);
    std::string next_str = format_rfc3339(job.next_run);
    sqlite3_bind_text(stmt, 12, next_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, job.id.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to update cron job: " + std::string(sqlite3_errmsg(db.get())));
    }

    return get_job(workspace_dir, job_id);
}

void record_last_run(
    const std::string& workspace_dir,
    const std::string& job_id,
    TimePoint finished_at,
    bool success,
    const std::string& output
) {
    std::string status = success ? "ok" : "error";
    std::string bounded_output = truncate_cron_output(output);

    SqlitePtr db(open_connection(workspace_dir));

    const char* sql = R"(
        UPDATE cron_jobs
        SET last_run = ?1, last_status = ?2, last_output = ?3
        WHERE id = ?4
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return;

    std::string finished_str = format_rfc3339(finished_at);
    sqlite3_bind_text(stmt, 1, finished_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, bounded_output.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, job_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void reschedule_after_run(
    const std::string& workspace_dir,
    int /*max_tasks*/,
    const std::string& job_id,
    const Schedule& schedule,
    bool success,
    const std::string& output
) {
    auto now = std::chrono::system_clock::now();
    TimePoint next_run;
    try {
        next_run = next_run_for_schedule(schedule, now);
    } catch (...) {
        next_run = now + std::chrono::hours(1); // fallback
    }
    std::string status = success ? "ok" : "error";
    std::string bounded_output = truncate_cron_output(output);

    SqlitePtr db(open_connection(workspace_dir));

    const char* sql = R"(
        UPDATE cron_jobs
        SET next_run = ?1, last_run = ?2, last_status = ?3, last_output = ?4
        WHERE id = ?5
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return;

    std::string next_str = format_rfc3339(next_run);
    std::string now_str = format_rfc3339(now);
    sqlite3_bind_text(stmt, 1, next_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, now_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, bounded_output.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, job_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void record_run(
    const std::string& workspace_dir,
    int max_run_history,
    const std::string& job_id,
    TimePoint started_at,
    TimePoint finished_at,
    const std::string& status,
    const std::optional<std::string>& output,
    int64_t duration_ms
) {
    std::optional<std::string> bounded_output;
    if (output) bounded_output = truncate_cron_output(*output);

    SqlitePtr db(open_connection(workspace_dir));

    // Insert run
    {
        const char* sql = R"(
            INSERT INTO cron_runs (job_id, started_at, finished_at, status, output, duration_ms)
            VALUES (?1, ?2, ?3, ?4, ?5, ?6)
        )";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return;

        std::string start_str = format_rfc3339(started_at);
        std::string finish_str = format_rfc3339(finished_at);
        sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, start_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, finish_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, status.c_str(), -1, SQLITE_TRANSIENT);
        if (bounded_output) {
            sqlite3_bind_text(stmt, 5, bounded_output->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 5);
        }
        sqlite3_bind_int64(stmt, 6, duration_ms);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Prune old runs
    {
        int keep = std::max(max_run_history, 1);
        const char* sql = R"(
            DELETE FROM cron_runs
            WHERE job_id = ?1
              AND id NOT IN (
                SELECT id FROM cron_runs
                WHERE job_id = ?1
                ORDER BY started_at DESC, id DESC
                LIMIT ?2
              )
        )";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return;

        sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, keep);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

std::vector<CronRun> list_runs(const std::string& workspace_dir, const std::string& job_id, size_t limit) {
    SqlitePtr db(open_connection(workspace_dir));
    std::vector<CronRun> runs;
    int lim = static_cast<int>(std::max(limit, size_t{1}));

    const char* sql = R"(
        SELECT id, job_id, started_at, finished_at, status, output, duration_ms
        FROM cron_runs
        WHERE job_id = ?1
        ORDER BY started_at DESC, id DESC
        LIMIT ?2
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return runs;

    sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, lim);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CronRun run;
        run.id = sqlite3_column_int64(stmt, 0);
        run.job_id = get_text(stmt, 1);
        run.started_at = parse_rfc3339(get_text(stmt, 2));
        run.finished_at = parse_rfc3339(get_text(stmt, 3));
        run.status = get_text(stmt, 4);
        run.output = get_optional_text(stmt, 5);
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            run.duration_ms = sqlite3_column_int64(stmt, 6);
        }
        runs.push_back(run);
    }
    sqlite3_finalize(stmt);
    return runs;
}

}
