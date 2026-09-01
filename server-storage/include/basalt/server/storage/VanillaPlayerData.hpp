#pragma once

#include <basalt/server/storage/StorageNbt.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::server::storage {

/// Reads a vanilla `playerdata/<uuid>.dat` and maps it onto a `StoredPlayer`.
///
/// The file is gzipped binary NBT, so the caller passes the decompressor: this module has no
/// business linking one, and `wire-io` already has gzip. What comes out is vanilla's own field
/// names, which are not the ones used here (`playerGameType`, `foodLevel`, `XpP`), so the mapping is
/// explicit rather than reflective.
///
/// A word on what this is not: a one-way import, not a compatibility layer. Basalt does not write
/// this format, and nothing here tries to preserve what it cannot model.
namespace vanillaPlayerData {

using Decompressor = std::function<std::vector<std::uint8_t>(std::span<const std::uint8_t>)>;

/// Vanilla stores the UUID in the file name, not in the file. The dimension is a resource location
/// (`minecraft:overworld`) and becomes a plain world name here, since a Basalt world is a directory
/// under `worlds/`, not a dimension of one save.
[[nodiscard]] std::expected<StoredPlayer, StorageFormatError>
read(std::string_view uuid, std::string_view name, std::span<const std::uint8_t> gzippedBytes,
     const Decompressor& decompress, std::int64_t nowEpochSeconds, std::string_view worldName);

} // namespace vanillaPlayerData

} // namespace basalt::server::storage
