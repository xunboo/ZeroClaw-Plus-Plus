#include "static_files.hpp"
#include <unordered_map>
#include <mutex>

namespace zeroclaw::gateway::static_files {

namespace {
    std::unique_ptr<IStaticAssetProvider> g_asset_provider;
    std::mutex g_provider_mutex;
}

void set_asset_provider(std::unique_ptr<IStaticAssetProvider> provider) {
    std::lock_guard<std::mutex> lock(g_provider_mutex);
    g_asset_provider = std::move(provider);
}

static std::string guess_mime_type(const std::string& path) {
    auto dot_pos = path.rfind('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";
    }
    
    auto ext = path.substr(dot_pos + 1);
    static const std::unordered_map<std::string, std::string> mime_types = {
        {"html", "text/html; charset=utf-8"},
        {"css", "text/css; charset=utf-8"},
        {"js", "application/javascript; charset=utf-8"},
        {"json", "application/json; charset=utf-8"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"svg", "image/svg+xml"},
        {"ico", "image/x-icon"},
        {"woff", "font/woff"},
        {"woff2", "font/woff2"},
        {"ttf", "font/ttf"},
        {"eot", "application/vnd.ms-fontobject"},
        {"webp", "image/webp"},
        {"mp3", "audio/mpeg"},
        {"mp4", "video/mp4"},
        {"webm", "video/webm"},
    };
    
    auto it = mime_types.find(ext);
    if (it != mime_types.end()) {
        return it->second;
    }
    
    return "application/octet-stream";
}

http::Response handle_static(const http::Request& req) {
    auto path = req.path;
    auto prefix_pos = path.find("/_app/");
    if (prefix_pos != std::string::npos) {
        path = path.substr(prefix_pos + 6);
    }
    
    std::lock_guard<std::mutex> lock(g_provider_mutex);
    
    if (!g_asset_provider) {
        http::Response resp;
        resp.status_code = http::StatusCode::NOT_FOUND;
        resp.body = "Not found";
        resp.content_type = "text/plain";
        return resp;
    }
    
    auto asset = g_asset_provider->get(path);
    if (!asset) {
        http::Response resp;
        resp.status_code = http::StatusCode::NOT_FOUND;
        resp.body = "Not found";
        resp.content_type = "text/plain";
        return resp;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.content_type = asset->mime_type.empty() ? guess_mime_type(path) : asset->mime_type;
    
    if (asset->immutable) {
        resp.headers.set("Cache-Control", "public, max-age=31536000, immutable");
    } else {
        resp.headers.set("Cache-Control", "no-cache");
    }
    
    resp.body.assign(asset->data.begin(), asset->data.end());
    return resp;
}

http::Response handle_spa_fallback(const http::Request& req) {
    std::lock_guard<std::mutex> lock(g_provider_mutex);
    
    if (!g_asset_provider) {
        http::Response resp;
        resp.status_code = http::StatusCode::NOT_FOUND;
        resp.body = "Not found";
        resp.content_type = "text/plain";
        return resp;
    }
    
    auto asset = g_asset_provider->get("index.html");
    if (!asset) {
        http::Response resp;
        resp.status_code = http::StatusCode::NOT_FOUND;
        resp.body = "Not found";
        resp.content_type = "text/plain";
        return resp;
    }
    
    http::Response resp;
    resp.status_code = http::StatusCode::OK;
    resp.content_type = "text/html; charset=utf-8";
    resp.headers.set("Cache-Control", "no-cache");
    resp.body.assign(asset->data.begin(), asset->data.end());
    return resp;
}

}
