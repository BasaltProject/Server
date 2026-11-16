#pragma once

#include <basalt/server/world/region/ChunkNbt.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace basalt::server::world::region::chunkBlob {

/// The binary form a region keeps; empty when a section needs the wide NBT palettes, so the caller stores NBT.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> write(const Chunk& chunk);

/// Reads a blob; one from an older data version goes through its NBT form and the data fixers.
[[nodiscard]] std::expected<Chunk, ChunkNbtError> read(std::span<const std::uint8_t> blob, ChunkHeight bounds);

/// The data version a blob was saved at; 0 for anything else.
[[nodiscard]] std::int32_t dataVersionOf(std::span<const std::uint8_t> stored);

} // namespace basalt::server::world::region::chunkBlob
