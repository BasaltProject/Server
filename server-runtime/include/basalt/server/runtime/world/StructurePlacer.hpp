#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureBounds.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `PlaceCommand.placeStructure`, skipping the placement grid and the biome checks.
struct StructurePlacer {
    wire::common::Identifier id;
    std::function<std::optional<server::world::structure::StructureBoundingBox>(std::int64_t seed, std::int32_t chunkX,
                                                                                std::int32_t chunkZ)>
        boundsAt;
    std::function<void(server::world::Chunk& chunk, std::int64_t seed, std::int32_t startChunkX,
                       std::int32_t startChunkZ)>
        place;
};

} // namespace basalt::server::runtime::world
