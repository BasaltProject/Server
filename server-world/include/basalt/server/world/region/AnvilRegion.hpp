#pragma once

#include <basalt/server/world/region/LinearRegion.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace basalt::server::world::region {

/// Why an Anvil (`.mca`) region could not be read.
enum class AnvilError : std::uint8_t {
    TooShort,
    /// A sector offset or length that points outside the file.
    OutOfBounds,
    /// Gzip (type 1) or the `.mcc` indirection for oversized chunks: refused rather than skipped
    /// silently, neither having come up in practice.
    UnsupportedCompression,
    CorruptChunk,
};

[[nodiscard]] std::string_view describe(AnvilError error);

/// Reads an Anvil (`.mca`) region into a `LinearRegion`, for the one-time import an existing
/// vanilla, Paper or Spigot world needs. Layout taken from `linear.py`'s `open_region_anvil`: a
/// 4096-byte header of 1024 `(3-byte sector offset, 1-byte count)` entries, 1024 timestamps, then
/// chunks in 4096-byte sectors, each a big-endian length, a compression type byte and the NBT.
///
/// Only the container is transcoded: the NBT reaches `LinearRegion::setChunk` decompressed but
/// untouched.
namespace anvilRegion {

[[nodiscard]] std::expected<LinearRegion, AnvilError> read(std::span<const std::uint8_t> bytes);

} // namespace anvilRegion

} // namespace basalt::server::world::region
