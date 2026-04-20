#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::assets {
struct AssetError {
    std::string message;
};

struct DownloadArtifact {
    std::string url;
    std::string sha1;
    std::int64_t size = 0;
};

struct VersionMetadata {
    std::string id;
    DownloadArtifact client;
    DownloadArtifact server;

    [[nodiscard]] std::string toJson() const;
};

namespace versionManifest {
inline constexpr std::string_view kVersionManifestUrl =
    "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";

using Fetcher = std::function<std::optional<std::string>(std::string_view url)>;

[[nodiscard]] std::expected<VersionMetadata, AssetError> parse(std::string_view versionId, std::string_view json);

[[nodiscard]] std::expected<VersionMetadata, AssetError> resolve(std::string_view versionId, const Fetcher& fetch);

[[nodiscard]] std::expected<VersionMetadata, AssetError> resolveCached(std::string_view versionId, const Fetcher& fetch,
                                                                       const std::optional<std::string>& cacheRoot);
} // namespace versionManifest
} // namespace basalt::server::assets
