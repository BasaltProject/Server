#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/light/LightWindow.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world {

namespace chunkDataCodec {

[[nodiscard]] std::vector<std::uint8_t> encodeSections(const Chunk& chunk);

/// parity: `ClientboundChunksBiomesPacket.ChunkBiomeData.extractChunkData`.
[[nodiscard]] std::vector<std::uint8_t> encodeBiomes(const Chunk& chunk);

/// parity: `Heightmap` packs 256 values.
[[nodiscard]] std::vector<std::int64_t> encodeHeightmap(const Chunk& chunk);

struct LightData {
    std::vector<std::int64_t> skyLightMask{0};
    std::vector<std::int64_t> emptySkyLightMask{0};
    std::vector<std::vector<std::uint8_t>> skyLight;
    std::vector<std::int64_t> blockLightMask{0};
    std::vector<std::int64_t> emptyBlockLightMask{0};
    std::vector<std::vector<std::uint8_t>> blockLight;
};

[[nodiscard]] LightData encodeLight(Chunk& chunk, bool hasSkylight, const light::LightWindow& window);

[[nodiscard]] LightData encodeLight(Chunk& chunk, bool hasSkylight = true);

} // namespace chunkDataCodec

} // namespace basalt::server::world
