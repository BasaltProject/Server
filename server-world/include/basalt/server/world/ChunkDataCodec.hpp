#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/light/LightWindow.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world {

/// Turns a chunk into the wire bytes the chunk and light packets need.
///
/// Biomes come from each section's quart-resolution palette, and the sky light from the light
/// engine, which lights each column and spreads sideways from there.
namespace chunkDataCodec {

/// The sections, as `MapChunkPacket.chunkData` carries them.
[[nodiscard]] std::vector<std::uint8_t> encodeSections(const Chunk& chunk);

/// Just the biome containers, in section order: the body of one column in `chunks_biomes`.
///
/// parity: `ClientboundChunksBiomesPacket.ChunkBiomeData.extractChunkData`, which writes the same
/// containers `encodeSections` does and nothing else, so a client can swap them in place without
/// touching blocks or light.
[[nodiscard]] std::vector<std::uint8_t> encodeBiomes(const Chunk& chunk);

/// The motion-blocking heightmap, packed the way vanilla's own does.
///
/// parity: `Heightmap` packs 256 values, relative to the chunk minimum rather than absolute, into a
/// storage of `ceillog2(height + 1)` bits each. Sending one raw long per column instead does not
/// crash but is silently ignored by the client ("size does not match; expected: 37, got: 256"),
/// which leaves its own heightmap unprimed and the loading screen never dismisses.
[[nodiscard]] std::vector<std::int64_t> encodeHeightmap(const Chunk& chunk);

/// The two masks and the arrays the light packet carries, for each of sky and block light.
struct LightData {
    std::vector<std::int64_t> skyLightMask{0};
    std::vector<std::int64_t> emptySkyLightMask{0};
    std::vector<std::vector<std::uint8_t>> skyLight;
    std::vector<std::int64_t> blockLightMask{0};
    std::vector<std::int64_t> emptyBlockLightMask{0};
    std::vector<std::vector<std::uint8_t>> blockLight;
};

/// The light of one chunk, section by section, computed once and kept on the chunk itself.
///
/// The window is the chunk and its eight neighbours: light reaches less than a chunk, so that is
/// everything which can shine into this one.
[[nodiscard]] LightData encodeLight(Chunk& chunk, bool hasSkylight, const light::LightWindow& window);

/// The same for a chunk with nothing loaded around it, which reads as open sky.
[[nodiscard]] LightData encodeLight(Chunk& chunk, bool hasSkylight = true);

} // namespace chunkDataCodec

} // namespace basalt::server::world
