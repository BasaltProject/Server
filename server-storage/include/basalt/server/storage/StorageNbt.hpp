#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace basalt::server::storage {

/// A stored blob that cannot be read back: a format from another version, or a corrupted one.
struct StorageFormatError {
    std::string message;
};

/// Turns an inventory into the blob that goes in the database, and back. NBT rather than JSON for
/// the reason vanilla uses it: item data is nested and typed, and JSON either loses every number's
/// type or reinvents NBT. Uncompressed, so `sqlite3` can still show it when something goes wrong.
///
/// The format version rides in every blob so a later change to the item model is detected rather
/// than misread: an unknown version is refused instead of loading someone's inventory as garbage.
namespace inventoryNbt {

inline constexpr std::int32_t kFormatVersion = 1;

[[nodiscard]] std::vector<std::uint8_t> encode(const StoredInventory& inventory);

[[nodiscard]] std::expected<StoredInventory, StorageFormatError> decode(std::span<const std::uint8_t> bytes);

} // namespace inventoryNbt

/// The stats blob, in the same shape and for the same reasons as the inventory one.
namespace statsNbt {

inline constexpr std::int32_t kFormatVersion = 1;

/// Empty stats encode to nothing at all, which is what goes in the column: a null blob.
[[nodiscard]] std::vector<std::uint8_t> encode(const StoredStats& stats);

[[nodiscard]] std::expected<StoredStats, StorageFormatError> decode(std::span<const std::uint8_t> bytes);

} // namespace statsNbt

} // namespace basalt::server::storage
