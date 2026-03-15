#pragma once

#include "gateway.hpp"
#include <string>
#include <optional>
#include <vector>

namespace zeroclaw::gateway::static_files {

struct StaticAsset {
    std::vector<uint8_t> data;
    std::string mime_type;
    bool immutable;
};

class IStaticAssetProvider {
public:
    virtual ~IStaticAssetProvider() = default;
    virtual std::optional<StaticAsset> get(const std::string& path) const = 0;
};

http::Response handle_static(const http::Request& req);
http::Response handle_spa_fallback(const http::Request& req);

void set_asset_provider(std::unique_ptr<IStaticAssetProvider> provider);

}
