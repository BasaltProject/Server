#include <basalt/server/assets/AssetCache.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <cstdint>
#include <format>
#include <mbedtls/sha1.h>
#include <utility>

namespace basalt::server::assets {

AssetCache::AssetCache(ByteFetcher fetch, std::string cacheRoot, wire::common::Logger logger)
    : fetch_(std::move(fetch)), cacheRoot_(std::move(cacheRoot)), logger_(std::move(logger)) {}

std::string AssetCache::sha1Hex(std::span<const std::uint8_t> bytes) {
    std::array<unsigned char, 20> digest{};
    mbedtls_sha1_context context{};
    mbedtls_sha1_init(&context);
    mbedtls_sha1_starts(&context);
    mbedtls_sha1_update(&context, bytes.data(), bytes.size());
    mbedtls_sha1_finish(&context, digest.data());
    mbedtls_sha1_free(&context);

    std::string hex;
    hex.reserve(digest.size() * 2);
    for (const unsigned char byte : digest) {
        hex += std::format("{:02x}", byte);
    }
    return hex;
}

std::expected<std::string, AssetError> AssetCache::ensure(const VersionMetadata& metadata, Artifact artifact) {
    const DownloadArtifact& download = artifact == Artifact::Client ? metadata.client : metadata.server;
    const std::string versionDir = cacheRoot_ + "/" + metadata.id;
    const std::string path = versionDir + "/" + (artifact == Artifact::Client ? "client.jar" : "server.jar");

    const std::optional<std::string> sidecar = wire::io::fileIO::readText(path + ".verified");
    const std::string expected = download.sha1 + " " + std::to_string(download.size);
    if (sidecar.has_value()) {
        std::string_view trimmed{*sidecar};
        while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r' || trimmed.back() == ' ')) {
            trimmed.remove_suffix(1);
        }
        const std::optional<std::uint64_t> size = wire::io::fileIO::size(path);
        if (trimmed == expected && size.has_value() && static_cast<std::int64_t>(*size) == download.size) {
            logger_.info([&] { return std::format("{} assets already downloaded and verified.", metadata.id); });
            return path;
        }
    }

    // One line that redraws itself rather than one per step: the console keeps the download on the
    // line it announced it on, and a log file gets the announcement once and none of the redraws.
    const std::string announcement =
        std::format("Downloading {} assets ({} MB)...", metadata.id, download.size / 1000000);
    // The first draw carries no percentage, so a log file and a console with no escape codes are
    // left with exactly the line they had before anything could be drawn over it.
    logger_.progress([&announcement] { return announcement; });
    std::int32_t drawn = -1;
    const std::optional<std::vector<std::uint8_t>> bytes =
        fetch_(download.url, [this, &announcement, &drawn, total = download.size](std::size_t soFar) {
            // Headers count towards `soFar` and the manifest's size does not, so the last stretch
            // would read past a hundred without the clamp.
            const std::int32_t percent =
                total <= 0 ? 0
                           : static_cast<std::int32_t>(std::min<std::uint64_t>(
                                 100, (static_cast<std::uint64_t>(soFar) * 100) / static_cast<std::uint64_t>(total)));
            if (percent != drawn) {
                drawn = percent;
                logger_.progress([&announcement, percent] { return std::format("{} {}%", announcement, percent); });
            }
        });
    logger_.endProgress();
    if (!bytes.has_value()) {
        return std::unexpected(
            AssetError{std::format("could not download {} assets from {}", metadata.id, download.url)});
    }
    const std::string actualSha1 = sha1Hex(*bytes);
    if (actualSha1 != download.sha1) {
        return std::unexpected(AssetError{std::format("SHA-1 mismatch downloading {} assets: expected {}, got {}",
                                                      metadata.id, download.sha1, actualSha1)});
    }

    wire::io::fileIO::createDirectories(versionDir);
    wire::io::fileIO::writeBytes(path, *bytes);
    wire::io::fileIO::writeText(path + ".verified", actualSha1 + " " + std::to_string(bytes->size()));
    logger_.info([&] { return std::format("{} assets downloaded and verified.", metadata.id); });
    return path;
}

} // namespace basalt::server::assets
