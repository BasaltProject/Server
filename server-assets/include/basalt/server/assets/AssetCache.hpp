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

/// A local, versioned cache for official Mojang artifacts: `./.basalt/assets/{version}/`, outside
/// the repo tree and gitignored. Never redistributed, never committed: this only ever writes into
/// that cache directory.
class AssetCache {
public:
    /// Which of a version's two published artifacts to fetch: server (`data/`) or client
    /// (`assets/`, `lang/`).
    enum class Artifact : std::uint8_t { Client, Server };

    /// Told how many bytes have arrived so far, as they do. A jar is tens of megabytes over a link
    /// nobody controls, so the wait is long enough that saying nothing about it reads as a hang.
    using Progress = std::function<void(std::size_t bytesSoFar)>;

    /// Answers the bytes at a URL, or nothing when they could not be fetched. A hook for the same
    /// reason the manifest's is: this module does not decide how the server talks to the network.
    using ByteFetcher =
        std::function<std::optional<std::vector<std::uint8_t>>(std::string_view url, const Progress& onProgress)>;

    explicit AssetCache(ByteFetcher fetch, std::string cacheRoot = "./.basalt/assets",
                        wire::common::Logger logger = wire::common::Logger{"AssetCache"});

    /// The local path to the cached, verified jar, downloading it first if needed.
    ///
    /// Hashing happens once, at download time, and lands in a `.verified` sidecar: re-hashing tens
    /// of megabytes every boot costs startup time and proves nothing new. The size is re-checked
    /// because a seek is free and catches the realistic corruption, a truncated download; an
    /// in-place edit of the same length is the trade. A missing or mismatched sidecar re-downloads.
    [[nodiscard]] std::expected<std::string, AssetError> ensure(const VersionMetadata& metadata, Artifact artifact);

    /// The hex SHA-1 of `bytes`, which is the digest Mojang publishes for its own downloads.
    [[nodiscard]] static std::string sha1Hex(std::span<const std::uint8_t> bytes);

private:
    ByteFetcher fetch_;
    std::string cacheRoot_;
    wire::common::Logger logger_;
};

} // namespace basalt::server::assets
