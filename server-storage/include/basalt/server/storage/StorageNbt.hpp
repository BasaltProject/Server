#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace basalt::server::storage {
struct StorageFormatError {
    std::string message;
};

namespace inventoryNbt {
inline constexpr std::int32_t kFormatVersion = 1;

[[nodiscard]] std::vector<std::uint8_t> encode(const StoredInventory& inventory);

[[nodiscard]] std::expected<StoredInventory, StorageFormatError> decode(std::span<const std::uint8_t> bytes);
} // namespace inventoryNbt

namespace statsNbt {
inline constexpr std::int32_t kFormatVersion = 1;

[[nodiscard]] std::vector<std::uint8_t> encode(const StoredStats& stats);

[[nodiscard]] std::expected<StoredStats, StorageFormatError> decode(std::span<const std::uint8_t> bytes);
} // namespace statsNbt
} // namespace basalt::server::storage
