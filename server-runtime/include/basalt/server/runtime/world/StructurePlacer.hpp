#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureBounds.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `PlaceCommand.placeStructure`, which generates a structure at the chunk holding the
/// given position, skipping the placement grid and the biome checks. `boundsAt` reports what a
/// start there would cover, or nothing when the structure's own conditions reject the chunk;
/// `place` writes the part of that start landing in the chunk it is given, like the worldgen pass
/// does per chunk, without recording a start.
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
