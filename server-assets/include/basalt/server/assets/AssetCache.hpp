#pragma once

#include <basalt/server/assets/VersionMetadata.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace basalt::server::assets {
class AssetCache {
public:
    enum class Artifact : std::uint8_t { Client, Server };

    using Progress = std::function<void(std::size_t bytesSoFar)>;

    using ByteFetcher =
        std::function<std::optional<std::vector<std::uint8_t>>(std::string_view url, const Progress& onProgress)>;

    explicit AssetCache(ByteFetcher fetch, std::string cacheRoot = "./.basalt/assets",
                        wire::common::Logger logger = wire::common::Logger{"AssetCache"});

    [[nodiscard]] std::expected<std::string, AssetError> ensure(const VersionMetadata& metadata, Artifact artifact);

    [[nodiscard]] static std::string sha1Hex(std::span<const std::uint8_t> bytes);

private:
    ByteFetcher fetch_;
    std::string cacheRoot_;
    wire::common::Logger logger_;
};
} // namespace basalt::server::assets
