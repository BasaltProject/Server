#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::assets {

/// Something this module needed and could not get: a version that is not published, a hash that did
/// not match, a jar that is not shaped the way the bundler shapes it.
struct AssetError {
    std::string message;
};

/// One downloadable artifact from a version's metadata: `downloads.client` or `downloads.server`.
struct DownloadArtifact {
    std::string url;
    std::string sha1;
    std::int64_t size = 0;
};

/// The subset of a version's `{version}.json` metadata this project needs.
struct VersionMetadata {
    std::string id;
    DownloadArtifact client;
    DownloadArtifact server;

    /// The same shape piston-meta returns, so a cached copy parses through the same code path.
    [[nodiscard]] std::string toJson() const;
};

/// Resolves a Minecraft version id to its metadata through Mojang's public piston-meta API: the
/// manifest lists every version with a metadata URL, and that per-version metadata lists the real
/// download URLs and hashes.
///
/// The fetch is a hook rather than an HTTP client of this module's own. Nothing here should decide
/// how the server talks to the network, and a warm install never calls it at all.
namespace versionManifest {

inline constexpr std::string_view kVersionManifestUrl =
    "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";

/// Answers the body of a URL, or nothing when it could not be fetched.
using Fetcher = std::function<std::optional<std::string>(std::string_view url)>;

[[nodiscard]] std::expected<VersionMetadata, AssetError> parse(std::string_view versionId, std::string_view json);

[[nodiscard]] std::expected<VersionMetadata, AssetError> resolve(std::string_view versionId, const Fetcher& fetch);

/// The resolved metadata, from the cache if it was resolved once already. Without this the server
/// makes two piston-meta round trips on every boot, and cannot start at all without a working
/// network connection even when both jars are downloaded and verified.
[[nodiscard]] std::expected<VersionMetadata, AssetError> resolveCached(std::string_view versionId, const Fetcher& fetch,
                                                                       const std::optional<std::string>& cacheRoot);

} // namespace versionManifest

} // namespace basalt::server::assets
