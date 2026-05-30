#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// Whether a neighbouring chunk's stored biomes include one (chunkX, chunkZ, biome id); empty when it cannot tell.
using ExternalBiomeReader = std::function<std::optional<bool>(std::int32_t, std::int32_t, std::int32_t)>;

/// parity: `ChunkGenerator.applyBiomeDecoration` only runs the features some biome of the 3x3 chunks lists, read from
/// their stored biomes. Empty when a neighbour is not available.
[[nodiscard]] std::optional<bool> biomeInStoredRange(const Chunk& chunk, std::string_view biome,
                                                     const ExternalBiomeReader& externalBiome);

} // namespace basalt::server::world::terrain::decoration
