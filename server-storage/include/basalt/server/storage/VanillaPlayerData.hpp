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
namespace vanillaPlayerData {
using Decompressor = std::function<std::vector<std::uint8_t>(std::span<const std::uint8_t>)>;

[[nodiscard]] std::expected<StoredPlayer, StorageFormatError>
read(std::string_view uuid, std::string_view name, std::span<const std::uint8_t> gzippedBytes,
     const Decompressor& decompress, std::int64_t nowEpochSeconds, std::string_view worldName);
} // namespace vanillaPlayerData
} // namespace basalt::server::storage
