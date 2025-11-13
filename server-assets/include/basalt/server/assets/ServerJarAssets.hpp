#pragma once

#include <basalt/server/assets/VersionMetadata.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::assets {

/// The official server's `data/minecraft/...` entries and its language files, unpacked into a data
/// root once and read as plain files from then on. The archive is deleted right after: keeping it
/// meant re-inflating a much larger download on every boot for a few megabytes.
///
/// Vanilla entries lose the `data/minecraft/` prefix while callers keep the full names; other
/// namespaces keep their path and cannot collide. `assets/minecraft/lang/` comes out too, since the
/// console resolves the same translation keys it sends clients.
class ServerJarAssets {
public:
    /// Where the archive is, resolved only when the install is cold: a warm one never asks, let
    /// alone contacts the network.
    using Source = std::function<std::expected<std::string, AssetError>()>;

    [[nodiscard]] static std::expected<ServerJarAssets, AssetError>
    open(std::string_view versionId, std::string dataRoot, const Source& source,
         wire::common::Logger logger = wire::common::Logger{"Assets"});

    /// One entry's raw bytes, such as `data/minecraft/worldgen/noise/continentalness.json`.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> readEntry(std::string_view name) const;

    /// One entry as text, refused rather than empty when absent: for datapack JSON callers expect to
    /// always exist.
    [[nodiscard]] std::expected<std::string, AssetError> readText(std::string_view name) const;

    /// Every entry name under `prefix`, for enumerating a whole datapack directory without knowing
    /// the file list ahead of time. Served from the index written at unpack time, since `wire-io`
    /// has no directory listing.
    [[nodiscard]] std::vector<std::string> entriesUnder(std::string_view prefix) const;

    /// Every id under `data/<namespace>/<directory>/`, which is how one registry directory is
    /// listed: what a command completes a registry argument with.
    [[nodiscard]] std::vector<wire::common::Identifier> dataIds(std::string_view directory,
                                                                std::string_view extension) const;

private:
    ServerJarAssets(std::string dataRoot, std::vector<std::string> entryNames, wire::common::Logger logger)
        : dataRoot_(std::move(dataRoot)), entryNames_(std::move(entryNames)), logger_(std::move(logger)) {}

    [[nodiscard]] std::string pathFor(std::string_view name) const;

    std::string dataRoot_;
    std::vector<std::string> entryNames_;
    wire::common::Logger logger_;
};

} // namespace basalt::server::assets
