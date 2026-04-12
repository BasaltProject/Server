#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/SpawnPlaces.hpp>
#include <basalt/server/world/structure/StructureQueries.hpp>

namespace basalt::server::runtime::world::spawnPlaces {

float moonBrightness(const LoadedWorld& world) {
    return naturalSpawner::surfaceSlimeChance(world.clock().totalTicks()) * 2.0F;
}

std::function<bool(const wire::common::Identifier&, std::int32_t, std::int32_t, std::int32_t)>
structureAt(LoadedWorld& world) {
    return [&world](const wire::common::Identifier& structure, std::int32_t x, std::int32_t y, std::int32_t z) {
        const server::world::structure::StructureQueries structures{
            [&world](std::int32_t chunkX, std::int32_t chunkZ) -> const server::world::structure::ChunkStructures* {
                const server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ);
                return chunk == nullptr ? nullptr : &chunk->structures();
            }};
        return structures.startWithPieceAt(server::world::structure::StructurePos{x, y, z}, structure.path()) !=
               nullptr;
    };
}

} // namespace basalt::server::runtime::world::spawnPlaces
